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
#include "../vk_debug.h"

bool WrappedVulkan::IsDrawInRenderPass()
{
  ResourceId rp;

  if(IsLoading(m_State))
    rp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass;
  else
    rp = m_RenderState.renderPass;

  ResourceId cmdid = m_LastCmdBufferID;

  bool rpActive = true;

  if(IsActiveReplaying(m_State))
  {
    cmdid = GetResID(RerecordCmdBuf(cmdid));

    rpActive =
        m_Partial[m_BakedCmdBufferInfo[cmdid].level == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? Primary : Secondary]
            .renderPassActive;
  }

  if(m_BakedCmdBufferInfo[cmdid].level == VK_COMMAND_BUFFER_LEVEL_PRIMARY &&
     (rp == ResourceId() || !rpActive))
  {
    return false;
  }
  else if(m_BakedCmdBufferInfo[cmdid].level == VK_COMMAND_BUFFER_LEVEL_SECONDARY &&
          (m_BakedCmdBufferInfo[cmdid].beginFlags &
           VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) == 0 &&
          (rp == ResourceId() || !rpActive))
  {
    return false;
  }

  // assume a secondary buffer with RENDER_PASS_CONTINUE_BIT is in a render pass.

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

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID) &&
         IsDrawInRenderPass())
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer);

        ObjDisp(commandBuffer)
            ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex, firstInstance);

        if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex,
                        firstInstance);
          m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
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

  ObjDisp(commandBuffer)
      ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex, firstInstance);

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

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID) &&
         IsDrawInRenderPass())
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer);

        ObjDisp(commandBuffer)
            ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex,
                             vertexOffset, firstInstance);

        if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex,
                               vertexOffset, firstInstance);
          m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
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

        draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced;

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

  ObjDisp(commandBuffer)
      ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex, vertexOffset,
                       firstInstance);

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

        if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID) &&
           IsDrawInRenderPass())
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t eventID = HandlePreCallback(commandBuffer);

          ObjDisp(commandBuffer)
              ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

          if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
            m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
          }
        }
      }
      else
      {
        if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
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

          RDCASSERT(it != m_DrawcallUses.end());

          uint32_t baseEventID = it->eventID;

          // when re-recording all, submit every drawcall individually to the callback
          if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds() && IsDrawInRenderPass())
          {
            for(uint32_t i = 0; i < count; i++)
            {
              uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);

              if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);
                m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
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
              // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
              // by just reducing the Count parameter to however many we want to replay. This only
              // works if we're replaying from the first multidraw to the nth (n less than Count)
              count = RDCMIN(count, m_LastEventID - baseEventID);
            }
            else
            {
              // otherwise we do the 'hard' case, draw only one multidraw
              // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
              // a single draw.
              drawidx = (curEID - baseEventID - 1);

              offset += stride * drawidx;
              count = 1;
            }

            if(IsDrawInRenderPass())
            {
              uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

              if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
                m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
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
      ObjDisp(commandBuffer)
          ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

      std::vector<byte> argbuf;
      GetDebugManager()->GetBufferData(
          GetResID(buffer), offset, sizeof(VkDrawIndirectCommand) + (count - 1) * stride, argbuf);

      string name = "vkCmdDrawIndirect(" + ToStr(count) + ")";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      // for 'single' draws, don't do complex multi-draw just inline it
      if(count <= 1)
      {
        DrawcallDescription draw;

        if(count == 1)
        {
          VkDrawIndirectCommand *args = (VkDrawIndirectCommand *)&argbuf[0];

          if(argbuf.size() >= sizeof(VkDrawIndirectCommand))
          {
            name += StringFormat::Fmt(" => <%u, %u>", args->vertexCount, args->instanceCount);

            draw.numIndices = args->vertexCount;
            draw.numInstances = args->instanceCount;
            draw.vertexOffset = args->firstVertex;
            draw.instanceOffset = args->firstInstance;
          }
          else
          {
            name += " => <?, ?>";
          }
        }

        AddEvent();

        draw.name = name;
        draw.flags = DrawFlags::Drawcall | DrawFlags::Instanced;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

        return true;
      }

      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;
      AddEvent();
      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(std::make_pair(
          GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      uint32_t cmdOffs = 0;

      for(uint32_t i = 0; i < count; i++)
      {
        VkDrawIndirectCommand params = {};
        bool valid = false;

        if(cmdOffs + sizeof(VkDrawIndirectCommand) <= argbuf.size())
        {
          params = *((VkDrawIndirectCommand *)&argbuf[cmdOffs]);
          valid = true;
          cmdOffs += sizeof(VkDrawIndirectCommand);
        }

        offset += stride;

        DrawcallDescription multi;
        multi.numIndices = params.vertexCount;
        multi.numInstances = params.instanceCount;
        multi.vertexOffset = params.firstVertex;
        multi.instanceOffset = params.firstInstance;

        multi.name = "vkCmdDrawIndirect[" + ToStr(i) + "](<" + ToStr(multi.numIndices) + ", " +
                     ToStr(multi.numInstances) + ">)";

        multi.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

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

void WrappedVulkan::vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                      VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirect);
    Serialise_vkCmdDrawIndirect(ser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
    if(GetRecord(buffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(buffer)->sparseInfo);
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

        if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID) &&
           IsDrawInRenderPass())
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t eventID = HandlePreCallback(commandBuffer);

          ObjDisp(commandBuffer)
              ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

          if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                         stride);
            m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
          }
        }
      }
      else
      {
        if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
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

          RDCASSERT(it != m_DrawcallUses.end());

          uint32_t baseEventID = it->eventID;

          // when re-recording all, submit every drawcall individually to the callback
          if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds() && IsDrawInRenderPass())
          {
            for(uint32_t i = 0; i < count; i++)
            {
              uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);

              if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1,
                                             stride);
                m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
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
              // if we're replaying part-way into a multidraw, we can replay the first part 'easily'
              // by just reducing the Count parameter to however many we want to replay. This only
              // works if we're replaying from the first multidraw to the nth (n less than Count)
              count = RDCMIN(count, m_LastEventID - baseEventID);
            }
            else
            {
              // otherwise we do the 'hard' case, draw only one multidraw
              // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
              // a single draw.
              drawidx = (curEID - baseEventID - 1);

              offset += stride * drawidx;
              count = 1;
            }

            if(IsDrawInRenderPass())
            {
              uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

              if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
                m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
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
      ObjDisp(commandBuffer)
          ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

      std::vector<byte> argbuf;
      GetDebugManager()->GetBufferData(GetResID(buffer), offset,
                                       sizeof(VkDrawIndexedIndirectCommand) + (count - 1) * stride,
                                       argbuf);

      string name = "vkCmdDrawIndexedIndirect(" + ToStr(count) + ")";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      // for 'single' draws, don't do complex multi-draw just inline it
      if(count <= 1)
      {
        DrawcallDescription draw;

        if(count == 1)
        {
          VkDrawIndexedIndirectCommand *args = (VkDrawIndexedIndirectCommand *)&argbuf[0];

          if(argbuf.size() >= sizeof(VkDrawIndexedIndirectCommand))
          {
            name += StringFormat::Fmt(" => <%u, %u>", args->indexCount, args->instanceCount);

            draw.numIndices = args->indexCount;
            draw.numInstances = args->instanceCount;
            draw.vertexOffset = args->vertexOffset;
            draw.indexOffset = args->firstIndex;
            draw.instanceOffset = args->firstInstance;
          }
          else
          {
            name += " => <?, ?>";
          }
        }

        AddEvent();

        draw.name = name;
        draw.flags = DrawFlags::Drawcall | DrawFlags::Instanced;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

        return true;
      }

      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;
      AddEvent();
      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(std::make_pair(
          GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      uint32_t cmdOffs = 0;

      for(uint32_t i = 0; i < count; i++)
      {
        VkDrawIndexedIndirectCommand params = {};
        bool valid = false;

        if(cmdOffs + sizeof(VkDrawIndexedIndirectCommand) <= argbuf.size())
        {
          params = *((VkDrawIndexedIndirectCommand *)&argbuf[cmdOffs]);
          valid = true;
          cmdOffs += sizeof(VkDrawIndexedIndirectCommand);
        }

        offset += stride;

        DrawcallDescription multi;
        multi.numIndices = params.indexCount;
        multi.numInstances = params.instanceCount;
        multi.vertexOffset = params.vertexOffset;
        multi.indexOffset = params.firstIndex;
        multi.instanceOffset = params.firstInstance;

        multi.name = "vkCmdDrawIndexedIndirect[" + ToStr(i) + "](<" + ToStr(multi.numIndices) +
                     ", " + ToStr(multi.numInstances) + ">)";

        multi.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

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

void WrappedVulkan::vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                             VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)
      ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexedIndirect);
    Serialise_vkCmdDrawIndexedIndirect(ser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
    if(GetRecord(buffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(buffer)->sparseInfo);
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

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

        ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

        if(eventID && m_DrawcallCallback->PostDispatch(eventID, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);
          m_DrawcallCallback->PostRedispatch(eventID, commandBuffer);
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

  ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

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

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

        ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

        if(eventID && m_DrawcallCallback->PostDispatch(eventID, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);
          m_DrawcallCallback->PostRedispatch(eventID, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

      {
        VkDispatchIndirectCommand unknown = {0};
        std::vector<byte> argbuf;
        GetDebugManager()->GetBufferData(GetResID(buffer), offset,
                                         sizeof(VkDispatchIndirectCommand), argbuf);
        VkDispatchIndirectCommand *args = (VkDispatchIndirectCommand *)&argbuf[0];

        if(argbuf.size() < sizeof(VkDispatchIndirectCommand))
        {
          RDCERR("Couldn't fetch arguments buffer for vkCmdDispatchIndirect");
          args = &unknown;
        }

        AddEvent();

        DrawcallDescription draw;
        draw.name =
            StringFormat::Fmt("vkCmdDispatchIndirect(<%u, %u, %u>", args->x, args->y, args->z);
        draw.dispatchDimension[0] = args->x;
        draw.dispatchDimension[1] = args->y;
        draw.dispatchDimension[2] = args->z;

        draw.flags |= DrawFlags::Dispatch | DrawFlags::Indirect;

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                          VkDeviceSize offset)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatchIndirect);
    Serialise_vkCmdDispatchIndirect(ser, commandBuffer, buffer, offset);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
    if(GetRecord(buffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(buffer)->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);
  SERIALISE_ELEMENT(filter);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Resolve);

        ObjDisp(commandBuffer)
            ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                           Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

        if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Resolve, commandBuffer);
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
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::Resolve)));
        }
        else
        {
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::ResolveSrc)));
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(destImage), EventUsage(drawNode.draw.eventID, ResourceUsage::ResolveSrc)));
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

  ObjDisp(commandBuffer)
      ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage),
                     destImageLayout, regionCount, pRegions, filter);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBlitImage);
    Serialise_vkCmdBlitImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                             destImageLayout, regionCount, pRegions, filter);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
    record->cmdInfo->dirtied.insert(GetResID(destImage));
    if(GetRecord(srcImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
    if(GetRecord(destImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Resolve);

        ObjDisp(commandBuffer)
            ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                              Unwrap(destImage), destImageLayout, regionCount, pRegions);

        if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                Unwrap(destImage), destImageLayout, regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Resolve, commandBuffer);
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
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::Resolve)));
        }
        else
        {
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::ResolveSrc)));
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(destImage), EventUsage(drawNode.draw.eventID, ResourceUsage::ResolveDst)));
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

  ObjDisp(commandBuffer)
      ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage),
                        destImageLayout, regionCount, pRegions);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdResolveImage);
    Serialise_vkCmdResolveImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                                destImageLayout, regionCount, pRegions);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
    record->cmdInfo->dirtied.insert(GetResID(destImage));
    if(GetRecord(srcImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
    if(GetRecord(destImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                           Unwrap(destImage), destImageLayout, regionCount, pRegions);

        if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destImage), destImageLayout, regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
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
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(destImage), EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
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

  ObjDisp(commandBuffer)
      ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage),
                     destImageLayout, regionCount, pRegions);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImage);
    Serialise_vkCmdCopyImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                             destImageLayout, regionCount, pRegions);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
    record->cmdInfo->dirtied.insert(GetResID(destImage));
    if(GetRecord(srcImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
    if(GetRecord(destImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                   destImageLayout, regionCount, pRegions);

        if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                     destImageLayout, regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
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

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(srcBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(destImage), EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
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

  ObjDisp(commandBuffer)
      ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                             destImageLayout, regionCount, pRegions);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBufferToImage);
    Serialise_vkCmdCopyBufferToImage(ser, commandBuffer, srcBuffer, destImage, destImageLayout,
                                     regionCount, pRegions);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(srcBuffer)->baseResource, eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
    record->cmdInfo->dirtied.insert(GetResID(destImage));
    if(GetRecord(srcBuffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(srcBuffer)->sparseInfo);
    if(GetRecord(destImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
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
  SERIALISE_ELEMENT(destBuffer);
  SERIALISE_ELEMENT(srcImage);
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                   Unwrap(destBuffer), regionCount, pRegions);

        if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                     Unwrap(destBuffer), regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
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

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(srcImage), EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(destBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
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

  ObjDisp(commandBuffer)
      ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destBuffer), regionCount, pRegions);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImageToBuffer);
    Serialise_vkCmdCopyImageToBuffer(ser, commandBuffer, srcImage, srcImageLayout, destBuffer,
                                     regionCount, pRegions);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);

    VkResourceRecord *buf = GetRecord(destBuffer);

    // mark buffer just as read, and memory behind as write & dirtied
    record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
    record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
    if(buf->baseResource != ResourceId())
      record->cmdInfo->dirtied.insert(buf->baseResource);
    if(GetRecord(srcImage)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
    if(buf->sparseInfo)
      record->cmdInfo->sparse.insert(buf->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer),
                            regionCount, pRegions);

        if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer),
                              regionCount, pRegions);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
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
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(srcBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(std::make_pair(
              GetResID(destBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
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

  ObjDisp(commandBuffer)
      ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), regionCount,
                      pRegions);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBuffer);
    Serialise_vkCmdCopyBuffer(ser, commandBuffer, srcBuffer, destBuffer, regionCount, pRegions);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(srcBuffer)->baseResource, eFrameRef_Read);

    VkResourceRecord *buf = GetRecord(destBuffer);

    // mark buffer just as read, and memory behind as write & dirtied
    record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
    record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
    if(buf->baseResource != ResourceId())
      record->cmdInfo->dirtied.insert(buf->baseResource);
    if(GetRecord(srcBuffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(srcBuffer)->sparseInfo);
    if(buf->sparseInfo)
      record->cmdInfo->sparse.insert(buf->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRanges, rangeCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID =
            HandlePreCallback(commandBuffer, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor));

        ObjDisp(commandBuffer)
            ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                                 rangeCount, pRanges);

        if(eventID &&
           m_DrawcallCallback->PostMisc(
               eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                                   rangeCount, pRanges);

          m_DrawcallCallback->PostRemisc(
              eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor), commandBuffer);
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
                                      Color.float32[0], Color.float32[0], Color.float32[0]);
        draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(image));

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(image), EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
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

  ObjDisp(commandBuffer)
      ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, pColor, rangeCount,
                           pRanges);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearColorImage);
    Serialise_vkCmdClearColorImage(ser, commandBuffer, image, imageLayout, pColor, rangeCount,
                                   pRanges);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    if(GetRecord(image)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(image)->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pRanges, rangeCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(
            commandBuffer, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil));

        ObjDisp(commandBuffer)
            ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                        &DepthStencil, rangeCount, pRanges);

        if(eventID &&
           m_DrawcallCallback->PostMisc(
               eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                          &DepthStencil, rangeCount, pRanges);

          m_DrawcallCallback->PostRemisc(
              eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil), commandBuffer);
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
        draw.name = StringFormat::Fmt("vkCmdClearColorImage(%f, %u)", DepthStencil.depth,
                                      DepthStencil.stencil);
        draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(image));

        AddDrawcall(draw, true);

        VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(image), EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
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

  ObjDisp(commandBuffer)
      ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, pDepthStencil,
                                  rangeCount, pRanges);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearDepthStencilImage);
    Serialise_vkCmdClearDepthStencilImage(ser, commandBuffer, image, imageLayout, pDepthStencil,
                                          rangeCount, pRanges);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    if(GetRecord(image)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(image)->sparseInfo);
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
  SERIALISE_ELEMENT_ARRAY(pAttachments, attachmentCount);
  SERIALISE_ELEMENT_ARRAY(pRects, rectCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(ShouldRerecordCmd(m_LastCmdBufferID) && InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags(DrawFlags::Clear));

        ObjDisp(commandBuffer)
            ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                  pRects);

        if(eventID &&
           m_DrawcallCallback->PostMisc(eventID, DrawFlags(DrawFlags::Clear), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                    pRects);

          m_DrawcallCallback->PostRemisc(eventID, DrawFlags(DrawFlags::Clear), commandBuffer);
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

        string name = "vkCmdClearAttachments(";
        for(uint32_t a = 0; a < attachmentCount; a++)
          name += ToStr(pAttachments[a].colorAttachment);
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
                    std::make_pair(m_CreationInfo.m_ImageView[fb.attachments[att].view].image,
                                   EventUsage(drawNode.draw.eventID, ResourceUsage::Clear,
                                              fb.attachments[att].view)));
              }
            }
            else if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            {
              if(rp.subpasses[state.subpass].depthstencilAttachment >= 0)
              {
                att = (uint32_t)rp.subpasses[state.subpass].depthstencilAttachment;
                drawNode.resourceUsage.push_back(
                    std::make_pair(m_CreationInfo.m_ImageView[fb.attachments[att].view].image,
                                   EventUsage(drawNode.draw.eventID, ResourceUsage::Clear,
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

  ObjDisp(commandBuffer)
      ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount, pRects);

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
