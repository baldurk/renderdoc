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
      dataSize = sizeof(VkDrawIndirectCommand) + (count > 0 ? count - 1 : 0) * stride;
      break;
    case VkIndirectPatchType::DrawIndexedIndirect:
    case VkIndirectPatchType::DrawIndexedIndirectCount:
      dataSize = sizeof(VkDrawIndexedIndirectCommand) + (count > 0 ? count - 1 : 0) * stride;
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
      VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
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
  SERIALISE_ELEMENT(vertexCount);
  SERIALISE_ELEMENT(instanceCount);
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

        if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex,
                        firstInstance);
          m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdDraw(%u, %u)", vertexCount, instanceCount);
        draw.numIndices = vertexCount;
        draw.numInstances = instanceCount;
        draw.indexOffset = 0;
        draw.vertexOffset = firstVertex;
        draw.instanceOffset = firstInstance;

        draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

        AddDrawcall(draw, true);
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDraw);
    Serialise_vkCmdDraw(ser, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndexed(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               uint32_t indexCount, uint32_t instanceCount,
                                               uint32_t firstIndex, int32_t vertexOffset,
                                               uint32_t firstInstance)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(indexCount);
  SERIALISE_ELEMENT(instanceCount);
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

        if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex,
                               vertexOffset, firstInstance);
          m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdDrawIndexed(%u, %u)", indexCount, instanceCount);
        draw.numIndices = indexCount;
        draw.numInstances = instanceCount;
        draw.indexOffset = firstIndex;
        draw.baseVertex = vertexOffset;
        draw.instanceOffset = firstInstance;

        draw.flags |= DrawFlags::Drawcall | DrawFlags::Indexed | DrawFlags::Instanced;

        AddDrawcall(draw, true);
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexed);
    Serialise_vkCmdDrawIndexed(ser, commandBuffer, indexCount, instanceCount, firstIndex,
                               vertexOffset, firstInstance);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndirect(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkBuffer buffer, VkDeviceSize offset,
                                                uint32_t count, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(count);
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

        if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t eventId = HandlePreCallback(commandBuffer);

          ObjDisp(commandBuffer)
              ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

          if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
            m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
          }
        }

        // account for the fake indirect subcommand
        if(count > 0)
          m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
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

          DrawcallUse use(m_CurChunkOffset, 0);
          auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

          if(it == m_DrawcallUses.end())
          {
            RDCERR("Unexpected drawcall not found in uses vector, offset %llu", m_CurChunkOffset);
          }
          else
          {
            uint32_t baseEventID = it->eventId;

            // when we have a callback, submit every drawcall individually to the callback
            if(m_DrawcallCallback && IsDrawInRenderPass())
            {
              for(uint32_t i = 0; i < count; i++)
              {
                uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);

                if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);
                  m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
                    VK_WHOLE_SIZE,
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
                uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

                if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
                  m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
      m_IndirectBufferSize = RDCMAX(m_IndirectBufferSize, sizeof(VkDrawIndirectCommand) +
                                                              (count > 0 ? count - 1 : 0) * stride);

      std::string name = "vkCmdDrawIndirect";

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
        DrawcallDescription draw;

        AddEvent();

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command");
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("command"_lit, VkDrawIndirectCommand());
        }

        m_StructuredFile->chunks.insert(m_StructuredFile->chunks.size() - 1, fakeChunk);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

        AddEvent();

        draw.name = name;
        draw.flags = DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.indirectPatch = indirectPatch;

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

        return true;
      }

      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;

      if(count == 0)
      {
        draw.flags = DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;
        draw.name = name + "(0)";
      }

      AddEvent();
      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.indirectPatch = indirectPatch;

      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

      if(count > 0)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      for(uint32_t i = 0; i < count; i++)
      {
        DrawcallDescription multi;
        multi.drawIndex = i;

        multi.name = name;

        multi.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command");
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("command"_lit, VkDrawIndirectCommand());
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multi, true);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      if(count > 0)
      {
        draw.name = name;
        draw.flags = DrawFlags::PopMarker;
        AddDrawcall(draw, false);
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirect);
    Serialise_vkCmdDrawIndirect(ser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(count);
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

        if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t eventId = HandlePreCallback(commandBuffer);

          ObjDisp(commandBuffer)
              ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

          if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                         stride);
            m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
          }
        }

        // account for the fake indirect subcommand
        if(count > 0)
          m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
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

          DrawcallUse use(m_CurChunkOffset, 0);
          auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

          if(it == m_DrawcallUses.end())
          {
            RDCERR("Unexpected drawcall not found in uses vector, offset %llu", m_CurChunkOffset);
          }
          else
          {
            uint32_t baseEventID = it->eventId;

            // when we have a callback, submit every drawcall individually to the callback
            if(m_DrawcallCallback && IsDrawInRenderPass())
            {
              for(uint32_t i = 0; i < count; i++)
              {
                uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1,
                                             stride);

                if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1,
                                               stride);
                  m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
                    VK_WHOLE_SIZE,
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
                uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                             stride);

                if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                               stride);
                  m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
      m_IndirectBufferSize = RDCMAX(m_IndirectBufferSize, sizeof(VkDrawIndexedIndirectCommand) +
                                                              (count > 0 ? count - 1 : 0) * stride);

      std::string name = "vkCmdDrawIndexedIndirect";

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
        DrawcallDescription draw;

        AddEvent();

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command");
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("command"_lit, VkDrawIndexedIndirectCommand());
        }

        m_StructuredFile->chunks.insert(m_StructuredFile->chunks.size() - 1, fakeChunk);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

        AddEvent();

        draw.name = name;
        draw.flags =
            DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed | DrawFlags::Indirect;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.indirectPatch = indirectPatch;

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

        return true;
      }

      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;

      if(count == 0)
      {
        draw.name = name + "(0)";
        draw.flags =
            DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed | DrawFlags::Indirect;
      }

      AddEvent();
      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.indirectPatch = indirectPatch;

      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

      if(count > 0)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      for(uint32_t i = 0; i < count; i++)
      {
        DrawcallDescription multi;
        multi.drawIndex = i;

        multi.name = name;

        multi.flags |=
            DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed | DrawFlags::Indirect;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command");
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("command"_lit, VkDrawIndexedIndirectCommand());
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multi, true);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      if(count > 0)
      {
        draw.name = name;
        draw.flags = DrawFlags::PopMarker;
        AddDrawcall(draw, false);
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexedIndirect);
    Serialise_vkCmdDrawIndexedIndirect(ser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(z);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

        ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

        if(eventId && m_DrawcallCallback->PostDispatch(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);
          m_DrawcallCallback->PostRedispatch(eventId, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

      {
        AddEvent();

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdDispatch(%u, %u, %u)", x, y, z);
        draw.dispatchDimension[0] = x;
        draw.dispatchDimension[1] = y;
        draw.dispatchDimension[2] = z;

        draw.flags |= DrawFlags::Dispatch;

        AddDrawcall(draw, true);
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatch);
    Serialise_vkCmdDispatch(ser, commandBuffer, x, y, z);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDispatchIndirect(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                    VkDeviceSize offset)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

        ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

        if(eventId && m_DrawcallCallback->PostDispatch(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);
          m_DrawcallCallback->PostRedispatch(eventId, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = "vkCmdDispatchIndirect(<?, ?, ?>)";
        draw.dispatchDimension[0] = 0;
        draw.dispatchDimension[1] = 0;
        draw.dispatchDimension[2] = 0;

        draw.flags |= DrawFlags::Dispatch | DrawFlags::Indirect;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.indirectPatch = indirectPatch;

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatchIndirect);
    Serialise_vkCmdDispatchIndirect(ser, commandBuffer, buffer, offset);

    record->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(srcImage);
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destImage);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Resolve);

        ObjDisp(commandBuffer)
            ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                           Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

        if(eventId && m_DrawcallCallback->PostMisc(eventId, DrawFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags::Resolve, commandBuffer);
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

        DrawcallDescription draw;
        draw.name =
            StringFormat::Fmt("vkCmdBlitImage(%s, %s)", ToStr(srcid).c_str(), ToStr(dstid).c_str());
        draw.flags |= DrawFlags::Resolve;

        draw.copySource = srcid;
        draw.copyDestination = dstid;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        if(srcImage == destImage)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveSrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destImage), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveSrc)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBlitImage);
    Serialise_vkCmdBlitImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                             destImageLayout, regionCount, pRegions, filter);

    record->AddChunk(scope.Get());

    for(uint32_t i = 0; i < regionCount; i++)
    {
      const VkImageBlit &region = pRegions[i];

      ImageRange srcRange(region.srcSubresource);

      srcRange.offset = {std::min(region.srcOffsets[0].x, region.srcOffsets[1].x),
                         std::min(region.srcOffsets[0].y, region.srcOffsets[1].y),
                         std::min(region.srcOffsets[0].z, region.srcOffsets[1].z)};
      srcRange.extent = {
          (uint32_t)(std::max(region.srcOffsets[0].x, region.srcOffsets[1].x) - srcRange.offset.x),
          (uint32_t)(std::max(region.srcOffsets[0].y, region.srcOffsets[1].y) - srcRange.offset.y),
          (uint32_t)(std::max(region.srcOffsets[0].z, region.srcOffsets[1].z) - srcRange.offset.z)};

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = {std::min(region.dstOffsets[0].x, region.dstOffsets[1].x),
                         std::min(region.dstOffsets[0].y, region.dstOffsets[1].y),
                         std::min(region.dstOffsets[0].z, region.dstOffsets[1].z)};
      dstRange.extent = {
          (uint32_t)(std::max(region.dstOffsets[0].x, region.dstOffsets[1].x) - dstRange.offset.x),
          (uint32_t)(std::max(region.dstOffsets[0].y, region.dstOffsets[1].y) - dstRange.offset.y),
          (uint32_t)(std::max(region.dstOffsets[0].z, region.dstOffsets[1].z) - dstRange.offset.z)};

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
  SERIALISE_ELEMENT(srcImage);
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destImage);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Resolve);

        ObjDisp(commandBuffer)
            ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                              Unwrap(destImage), destImageLayout, regionCount, pRegions);

        if(eventId && m_DrawcallCallback->PostMisc(eventId, DrawFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                Unwrap(destImage), destImageLayout, regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags::Resolve, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdResolveImage(%s, %s)", ToStr(srcid).c_str(),
                                      ToStr(dstid).c_str());
        draw.flags |= DrawFlags::Resolve;

        draw.copySource = srcid;
        draw.copyDestination = dstid;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        if(srcImage == destImage)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveSrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destImage), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveDst)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdResolveImage);
    Serialise_vkCmdResolveImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                                destImageLayout, regionCount, pRegions);

    record->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(srcImage);
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destImage);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                           Unwrap(destImage), destImageLayout, regionCount, pRegions);

        if(eventId && m_DrawcallCallback->PostMisc(eventId, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destImage), destImageLayout, regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags::Copy, commandBuffer);
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

        DrawcallDescription draw;
        draw.name =
            StringFormat::Fmt("vkCmdCopyImage(%s, %s)", ToStr(srcid).c_str(), ToStr(dstid).c_str());
        draw.flags |= DrawFlags::Copy;

        draw.copySource = srcid;
        draw.copyDestination = dstid;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        if(srcImage == destImage)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destImage), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImage);
    Serialise_vkCmdCopyImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                             destImageLayout, regionCount, pRegions);

    record->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(srcBuffer);
  SERIALISE_ELEMENT(destImage);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                   destImageLayout, regionCount, pRegions);

        if(eventId && m_DrawcallCallback->PostMisc(eventId, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                     destImageLayout, regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags::Copy, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdCopyBufferToImage(%s, %s)", ToStr(bufid).c_str(),
                                      ToStr(imgid).c_str());
        draw.flags |= DrawFlags::Copy;

        draw.copySource = bufid;
        draw.copyDestination = imgid;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(srcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(destImage), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBufferToImage);
    Serialise_vkCmdCopyBufferToImage(ser, commandBuffer, srcBuffer, destImage, destImageLayout,
                                     regionCount, pRegions);

    record->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(srcImage);
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destBuffer);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                   Unwrap(destBuffer), regionCount, pRegions);

        if(eventId && m_DrawcallCallback->PostMisc(eventId, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                     Unwrap(destBuffer), regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags::Copy, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdCopyImageToBuffer(%s, %s)", ToStr(imgid).c_str(),
                                      ToStr(bufid).c_str());
        draw.flags |= DrawFlags::Copy;

        draw.copySource = imgid;
        draw.copyDestination = bufid;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(srcImage), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(destBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImageToBuffer);
    Serialise_vkCmdCopyImageToBuffer(ser, commandBuffer, srcImage, srcImageLayout, destBuffer,
                                     regionCount, pRegions);

    record->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(srcBuffer);
  SERIALISE_ELEMENT(destBuffer);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer),
                            regionCount, pRegions);

        if(eventId && m_DrawcallCallback->PostMisc(eventId, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer),
                              regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags::Copy, commandBuffer);
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

        DrawcallDescription draw;
        draw.name =
            StringFormat::Fmt("vkCmdCopyBuffer(%s, %s)", ToStr(srcid).c_str(), ToStr(dstid).c_str());
        draw.flags |= DrawFlags::Copy;

        draw.copySource = srcid;
        draw.copyDestination = dstid;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        if(srcBuffer == destBuffer)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBuffer);
    Serialise_vkCmdCopyBuffer(ser, commandBuffer, srcBuffer, destBuffer, regionCount, pRegions);

    record->AddChunk(scope.Get());
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
bool WrappedVulkan::Serialise_vkCmdClearColorImage(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   VkImage image, VkImageLayout imageLayout,
                                                   const VkClearColorValue *pColor,
                                                   uint32_t rangeCount,
                                                   const VkImageSubresourceRange *pRanges)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(image);
  SERIALISE_ELEMENT(imageLayout);
  SERIALISE_ELEMENT_LOCAL(Color, *pColor);
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

        uint32_t eventId =
            HandlePreCallback(commandBuffer, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor));

        ObjDisp(commandBuffer)
            ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                                 rangeCount, pRanges);

        if(eventId &&
           m_DrawcallCallback->PostMisc(
               eventId, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                                   rangeCount, pRanges);

          m_DrawcallCallback->PostRemisc(
              eventId, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor), commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdClearColorImage(%f, %f, %f, %f)", Color.float32[0],
                                      Color.float32[1], Color.float32[2], Color.float32[3]);
        draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(image));

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(
            make_rdcpair(GetResID(image), EventUsage(drawNode.draw.eventId, ResourceUsage::Clear)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearColorImage);
    Serialise_vkCmdClearColorImage(ser, commandBuffer, image, imageLayout, pColor, rangeCount,
                                   pRanges);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    record->cmdInfo->dirtied.insert(GetResID(image));
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
  SERIALISE_ELEMENT(image);
  SERIALISE_ELEMENT(imageLayout);
  SERIALISE_ELEMENT_LOCAL(DepthStencil, *pDepthStencil);
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
            commandBuffer, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil));

        ObjDisp(commandBuffer)
            ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                        &DepthStencil, rangeCount, pRanges);

        if(eventId &&
           m_DrawcallCallback->PostMisc(
               eventId, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                          &DepthStencil, rangeCount, pRanges);

          m_DrawcallCallback->PostRemisc(
              eventId, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil), commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdClearDepthStencilImage(%f, %u)", DepthStencil.depth,
                                      DepthStencil.stencil);
        draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(image));

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(
            make_rdcpair(GetResID(image), EventUsage(drawNode.draw.eventId, ResourceUsage::Clear)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearDepthStencilImage);
    Serialise_vkCmdClearDepthStencilImage(ser, commandBuffer, image, imageLayout, pDepthStencil,
                                          rangeCount, pRanges);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_PartialWrite);
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    record->cmdInfo->dirtied.insert(GetResID(image));
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
  SERIALISE_ELEMENT_ARRAY(pAttachments, attachmentCount);
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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags(DrawFlags::Clear));

        ObjDisp(commandBuffer)
            ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                  pRects);

        if(eventId &&
           m_DrawcallCallback->PostMisc(eventId, DrawFlags(DrawFlags::Clear), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                    pRects);

          m_DrawcallCallback->PostRemisc(eventId, DrawFlags(DrawFlags::Clear), commandBuffer);
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

        std::string name = "vkCmdClearAttachments(";
        for(uint32_t a = 0; a < attachmentCount; a++)
        {
          name += ToStr(pAttachments[a].colorAttachment);
          if(a + 1 < attachmentCount)
            name += ", ";
        }
        name += ")";

        DrawcallDescription draw;
        draw.name = name;
        draw.flags |= DrawFlags::Clear;
        for(uint32_t a = 0; a < attachmentCount; a++)
        {
          if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
            draw.flags |= DrawFlags::ClearColor;
          if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            draw.flags |= DrawFlags::ClearDepthStencil;
        }

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();
        const BakedCmdBufferInfo::CmdBufferState &state =
            m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

        if(state.renderPass != ResourceId() && state.framebuffer != ResourceId())
        {
          VulkanCreationInfo::RenderPass &rp = m_CreationInfo.m_RenderPass[state.renderPass];
          VulkanCreationInfo::Framebuffer &fb = m_CreationInfo.m_Framebuffer[state.framebuffer];

          RDCASSERT(state.subpass < rp.subpasses.size());

          for(uint32_t a = 0; a < attachmentCount; a++)
          {
            uint32_t att = pAttachments[a].colorAttachment;

            if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
            {
              if(att < (uint32_t)rp.subpasses[state.subpass].colorAttachments.size())
              {
                att = rp.subpasses[state.subpass].colorAttachments[att];
                drawNode.resourceUsage.push_back(
                    make_rdcpair(m_CreationInfo.m_ImageView[fb.attachments[att].view].image,
                                 EventUsage(drawNode.draw.eventId, ResourceUsage::Clear,
                                            fb.attachments[att].view)));
              }
            }
            else if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            {
              if(rp.subpasses[state.subpass].depthstencilAttachment >= 0)
              {
                att = (uint32_t)rp.subpasses[state.subpass].depthstencilAttachment;
                drawNode.resourceUsage.push_back(
                    make_rdcpair(m_CreationInfo.m_ImageView[fb.attachments[att].view].image,
                                 EventUsage(drawNode.draw.eventId, ResourceUsage::Clear,
                                            fb.attachments[att].view)));
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearAttachments);
    Serialise_vkCmdClearAttachments(ser, commandBuffer, attachmentCount, pAttachments, rectCount,
                                    pRects);

    record->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(groupCountX);
  SERIALISE_ELEMENT(groupCountY);
  SERIALISE_ELEMENT(groupCountZ);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

        ObjDisp(commandBuffer)
            ->CmdDispatchBase(Unwrap(commandBuffer), baseGroupX, baseGroupY, baseGroupZ,
                              groupCountX, groupCountY, groupCountZ);

        if(eventId && m_DrawcallCallback->PostDispatch(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDispatchBase(Unwrap(commandBuffer), baseGroupX, baseGroupY, baseGroupZ,
                                groupCountX, groupCountY, groupCountZ);
          m_DrawcallCallback->PostRedispatch(eventId, commandBuffer);
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

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("vkCmdDispatchBase(%u, %u, %u)", groupCountX, groupCountY,
                                      groupCountZ);
        draw.dispatchDimension[0] = groupCountX;
        draw.dispatchDimension[1] = groupCountY;
        draw.dispatchDimension[2] = groupCountZ;
        draw.dispatchBase[0] = baseGroupX;
        draw.dispatchBase[1] = baseGroupY;
        draw.dispatchBase[2] = baseGroupZ;

        draw.flags |= DrawFlags::Dispatch;

        AddDrawcall(draw, true);
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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatchBase);
    Serialise_vkCmdDispatchBase(ser, commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX,
                                groupCountY, groupCountZ);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndirectCountKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
    VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(countBuffer);
  SERIALISE_ELEMENT(countBufferOffset);
  SERIALISE_ELEMENT(maxDrawCount);
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

        DrawcallUse use(m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

        if(it == m_DrawcallUses.end() || GetDrawcall(it->eventId) == NULL)
        {
          RDCERR("Unexpected drawcall not found in uses vector, offset %llu", m_CurChunkOffset);
        }
        else
        {
          uint32_t baseEventID = it->eventId;

          // get the number of draws by looking at how many children the parent drawcall has.
          count = (uint32_t)GetDrawcall(it->eventId)->children.size();

          // when we have a callback, submit every drawcall individually to the callback
          if(m_DrawcallCallback && IsDrawInRenderPass())
          {
            for(uint32_t i = 0; i < count; i++)
            {
              uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);

              if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);
                m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
                  VK_WHOLE_SIZE,
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
              uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

              if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
                m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
          ->CmdDrawIndirectCountKHR(Unwrap(commandBuffer), Unwrap(buffer), offset,
                                    Unwrap(countBuffer), countBufferOffset, maxDrawCount, stride);

      // add on the size we'll need for an indirect buffer in the worst case.
      // Note that we'll only ever be partially replaying one draw at a time, so we only need the
      // worst case.
      m_IndirectBufferSize =
          RDCMAX(m_IndirectBufferSize,
                 sizeof(VkDrawIndirectCommand) + (maxDrawCount > 0 ? maxDrawCount - 1 : 0) * stride);

      std::string name = "vkCmdDrawIndirectCountKHR";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;

      if(maxDrawCount == 0)
        draw.name = name + "(0)";

      AddEvent();
      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.indirectPatch = indirectPatch;

      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      for(uint32_t i = 0; i < maxDrawCount; i++)
      {
        DrawcallDescription multi;
        multi.drawIndex = i;

        multi.name = name;

        multi.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command");
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("command"_lit, VkDrawIndirectCommand());
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multi, true);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      draw.name = name;
      draw.flags = DrawFlags::PopMarker;
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                              VkDeviceSize offset, VkBuffer countBuffer,
                                              VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                              uint32_t stride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDrawIndirectCountKHR(Unwrap(commandBuffer), Unwrap(buffer), offset,
                                                    Unwrap(countBuffer), countBufferOffset,
                                                    maxDrawCount, stride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirectCountKHR);
    Serialise_vkCmdDrawIndirectCountKHR(ser, commandBuffer, buffer, offset, countBuffer,
                                        countBufferOffset, maxDrawCount, stride);

    record->AddChunk(scope.Get());

    record->MarkBufferFrameReferenced(GetRecord(buffer), offset,
                                      stride * (maxDrawCount - 1) + sizeof(VkDrawIndirectCommand),
                                      eFrameRef_Read);
    record->MarkBufferFrameReferenced(GetRecord(countBuffer), countBufferOffset, 4, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirectCountKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
    VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(countBuffer);
  SERIALISE_ELEMENT(countBufferOffset);
  SERIALISE_ELEMENT(maxDrawCount);
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

        DrawcallUse use(m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

        if(it == m_DrawcallUses.end() || GetDrawcall(it->eventId) == NULL)
        {
          RDCERR("Unexpected drawcall not found in uses vector, offset %llu", m_CurChunkOffset);
        }
        else
        {
          uint32_t baseEventID = it->eventId;

          // get the number of draws by looking at how many children the parent drawcall has.
          count = (uint32_t)GetDrawcall(it->eventId)->children.size();

          // when we have a callback, submit every drawcall individually to the callback
          if(m_DrawcallCallback && IsDrawInRenderPass())
          {
            for(uint32_t i = 0; i < count; i++)
            {
              uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);

              if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1,
                                             stride);
                m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
                  VK_WHOLE_SIZE,
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
              uint32_t eventId = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                           stride);

              if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                             stride);
                m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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
          ->CmdDrawIndexedIndirectCountKHR(Unwrap(commandBuffer), Unwrap(buffer), offset,
                                           Unwrap(countBuffer), countBufferOffset, maxDrawCount,
                                           stride);

      // add on the size we'll need for an indirect buffer in the worst case.
      // Note that we'll only ever be partially replaying one draw at a time, so we only need the
      // worst case.
      m_IndirectBufferSize =
          RDCMAX(m_IndirectBufferSize, sizeof(VkDrawIndexedIndirectCommand) +
                                           (maxDrawCount > 0 ? maxDrawCount - 1 : 0) * stride);

      std::string name = "vkCmdDrawIndexedIndirectCountKHR";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;

      if(maxDrawCount == 0)
        draw.name = name + "(0)";

      AddEvent();
      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.indirectPatch = indirectPatch;

      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      for(uint32_t i = 0; i < maxDrawCount; i++)
      {
        DrawcallDescription multi;
        multi.drawIndex = i;

        multi.name = name;

        multi.flags |=
            DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed | DrawFlags::Indirect;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command");
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("command"_lit, VkDrawIndexedIndirectCommand());
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddDrawcall(multi, true);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      draw.name = name;
      draw.flags = DrawFlags::PopMarker;
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                     VkDeviceSize offset, VkBuffer countBuffer,
                                                     VkDeviceSize countBufferOffset,
                                                     uint32_t maxDrawCount, uint32_t stride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDrawIndexedIndirectCountKHR(Unwrap(commandBuffer), Unwrap(buffer),
                                                           offset, Unwrap(countBuffer),
                                                           countBufferOffset, maxDrawCount, stride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexedIndirectCountKHR);
    Serialise_vkCmdDrawIndexedIndirectCountKHR(ser, commandBuffer, buffer, offset, countBuffer,
                                               countBufferOffset, maxDrawCount, stride);

    record->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(instanceCount);
  SERIALISE_ELEMENT(firstInstance);
  SERIALISE_ELEMENT(counterBuffer);
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

        if(eventId && m_DrawcallCallback->PostDraw(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDrawIndirectByteCountEXT(Unwrap(commandBuffer), instanceCount, firstInstance,
                                            Unwrap(counterBuffer), counterBufferOffset,
                                            counterOffset, vertexStride);
          m_DrawcallCallback->PostRedraw(eventId, commandBuffer);
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

      std::string name = "vkCmdDrawIndirectByteCountEXT";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      DrawcallDescription draw;

      AddEvent();

      draw.name = name;
      draw.instanceOffset = firstInstance;
      draw.numInstances = instanceCount;
      draw.flags = DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.indirectPatch = indirectPatch;

      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(counterBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

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

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirectByteCountEXT);
    Serialise_vkCmdDrawIndirectByteCountEXT(ser, commandBuffer, instanceCount, firstInstance,
                                            counterBuffer, counterBufferOffset, counterOffset,
                                            vertexStride);

    record->AddChunk(scope.Get());

    record->MarkBufferFrameReferenced(GetRecord(counterBuffer), counterBufferOffset, 4,
                                      eFrameRef_Read);
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

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndirectCountKHR, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                uint32_t stride);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirectCountKHR,
                                VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                VkBuffer countBuffer, VkDeviceSize countBufferOffset,
                                uint32_t maxDrawCount, uint32_t stride);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndirectByteCountEXT, VkCommandBuffer commandBuffer,
                                uint32_t instanceCount, uint32_t firstInstance,
                                VkBuffer counterBuffer, VkDeviceSize counterBufferOffset,
                                uint32_t counterOffset, uint32_t vertexStride);
