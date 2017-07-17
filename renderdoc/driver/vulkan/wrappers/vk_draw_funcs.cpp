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

  if(m_State == READING)
    rp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass;
  else
    rp = m_RenderState.renderPass;

  ResourceId cmdid = m_LastCmdBufferID;

  bool rpActive = true;

  if(m_State == EXECUTING)
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

bool WrappedVulkan::Serialise_vkCmdDraw(Serialiser *localSerialiser, VkCommandBuffer commandBuffer,
                                        uint32_t vertexCount, uint32_t instanceCount,
                                        uint32_t firstVertex, uint32_t firstInstance)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(uint32_t, vtxCount, vertexCount);
  SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);
  SERIALISE_ELEMENT(uint32_t, firstVtx, firstVertex);
  SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid) && m_RenderState.renderPass != ResourceId())
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer);

      ObjDisp(commandBuffer)->CmdDraw(Unwrap(commandBuffer), vtxCount, instCount, firstVtx, firstInst);

      if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
      {
        ObjDisp(commandBuffer)->CmdDraw(Unwrap(commandBuffer), vtxCount, instCount, firstVtx, firstInst);
        m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(commandBuffer)->CmdDraw(Unwrap(commandBuffer), vtxCount, instCount, firstVtx, firstInst);

    const string desc = localSerialiser->GetDebugStr();

    if(!IsDrawInRenderPass())
    {
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                      MessageSource::IncorrectAPIUse,
                      "Drawcall in happening outside of render pass, or in secondary command "
                      "buffer without RENDER_PASS_CONTINUE_BIT");
    }

    {
      AddEvent(desc);
      string name = "vkCmdDraw(" + ToStr::Get(vtxCount) + "," + ToStr::Get(instCount) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.numIndices = vtxCount;
      draw.numInstances = instCount;
      draw.indexOffset = 0;
      draw.vertexOffset = firstVtx;
      draw.instanceOffset = firstInst;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      AddDrawcall(draw, true);
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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DRAW);
    Serialise_vkCmdDraw(localSerialiser, commandBuffer, vertexCount, instanceCount, firstVertex,
                        firstInstance);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexed(Serialiser *localSerialiser,
                                               VkCommandBuffer commandBuffer, uint32_t indexCount,
                                               uint32_t instanceCount, uint32_t firstIndex,
                                               int32_t vertexOffset, uint32_t firstInstance)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(uint32_t, idxCount, indexCount);
  SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);
  SERIALISE_ELEMENT(uint32_t, firstIdx, firstIndex);
  SERIALISE_ELEMENT(int32_t, vtxOffs, vertexOffset);
  SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid) && IsDrawInRenderPass())
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer);

      ObjDisp(commandBuffer)
          ->CmdDrawIndexed(Unwrap(commandBuffer), idxCount, instCount, firstIdx, vtxOffs, firstInst);

      if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdDrawIndexed(Unwrap(commandBuffer), idxCount, instCount, firstIdx, vtxOffs,
                             firstInst);
        m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(commandBuffer)
        ->CmdDrawIndexed(Unwrap(commandBuffer), idxCount, instCount, firstIdx, vtxOffs, firstInst);

    const string desc = localSerialiser->GetDebugStr();

    if(!IsDrawInRenderPass())
    {
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                      MessageSource::IncorrectAPIUse,
                      "Drawcall in happening outside of render pass, or in secondary command "
                      "buffer without RENDER_PASS_CONTINUE_BIT");
    }

    {
      AddEvent(desc);
      string name = "vkCmdDrawIndexed(" + ToStr::Get(idxCount) + "," + ToStr::Get(instCount) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.numIndices = idxCount;
      draw.numInstances = instCount;
      draw.indexOffset = firstIdx;
      draw.baseVertex = vtxOffs;
      draw.instanceOffset = firstInst;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced;

      AddDrawcall(draw, true);
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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED);
    Serialise_vkCmdDrawIndexed(localSerialiser, commandBuffer, indexCount, instanceCount,
                               firstIndex, vertexOffset, firstInstance);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdDrawIndirect(Serialiser *localSerialiser,
                                                VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
  SERIALISE_ELEMENT(uint64_t, offs, offset);

  SERIALISE_ELEMENT(uint32_t, cnt, count);
  SERIALISE_ELEMENT(uint32_t, strd, stride);

  bool multidraw = cnt > 1;

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  // do execution (possibly partial)
  if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
  }
  else if(m_State == EXECUTING && cnt <= 1)
  {
    // for single draws, it's pretty simple
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid) && IsDrawInRenderPass())
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer);

      ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);

      if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
      {
        ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
        m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
      }
    }
  }
  else if(m_State == EXECUTING)
  {
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

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
        for(uint32_t i = 0; i < cnt; i++)
        {
          uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

          ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, 1, strd);

          if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
          {
            ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, 1, strd);
            m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
          }

          offs += strd;
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
          cnt = RDCMIN(cnt, m_LastEventID - baseEventID);
        }
        else
        {
          // otherwise we do the 'hard' case, draw only one multidraw
          // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
          // a single draw.
          drawidx = (curEID - baseEventID - 1);

          offs += strd * drawidx;
          cnt = 1;
        }

        if(IsDrawInRenderPass())
        {
          uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

          ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);

          if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
            m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
          }
        }
      }
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages(localSerialiser, true);

  // while reading, create the drawcall set
  if(m_State == READING)
  {
    vector<byte> argbuf;
    GetDebugManager()->GetBufferData(GetResID(buffer), offs,
                                     sizeof(VkDrawIndirectCommand) + (cnt - 1) * strd, argbuf);

    string name = "vkCmdDrawIndirect(" + ToStr::Get(cnt) + ")";

    if(!IsDrawInRenderPass())
    {
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                      MessageSource::IncorrectAPIUse,
                      "Drawcall in happening outside of render pass, or in secondary command "
                      "buffer without RENDER_PASS_CONTINUE_BIT");
    }

    // for 'single' draws, don't do complex multi-draw just inline it
    if(cnt <= 1)
    {
      DrawcallDescription draw;

      if(cnt == 1)
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

      AddEvent(desc);

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
    AddEvent(desc);
    AddDrawcall(draw, true);

    VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

    drawNode.resourceUsage.push_back(std::make_pair(
        GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

    uint32_t cmdOffs = 0;

    for(uint32_t i = 0; i < cnt; i++)
    {
      VkDrawIndirectCommand params = {};
      bool valid = false;

      if(cmdOffs + sizeof(VkDrawIndirectCommand) <= argbuf.size())
      {
        params = *((VkDrawIndirectCommand *)&argbuf[cmdOffs]);
        valid = true;
        cmdOffs += sizeof(VkDrawIndirectCommand);
      }

      offs += strd;

      DrawcallDescription multi;
      multi.numIndices = params.vertexCount;
      multi.numInstances = params.instanceCount;
      multi.vertexOffset = params.firstVertex;
      multi.instanceOffset = params.firstInstance;

      multi.name = "vkCmdDrawIndirect[" + ToStr::Get(i) + "](<" + ToStr::Get(multi.numIndices) +
                   ", " + ToStr::Get(multi.numInstances) + ">)";

      multi.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

      AddEvent(multi.name.elems);
      AddDrawcall(multi, true);

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
    }

    draw.name = name;
    draw.flags = DrawFlags::PopMarker;
    AddDrawcall(draw, false);
  }
  else if(multidraw)
  {
    // multidraws skip the event ID past the whole thing
    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID += cnt + 1;
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                      VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DRAW_INDIRECT);
    Serialise_vkCmdDrawIndirect(localSerialiser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
    if(GetRecord(buffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(buffer)->sparseInfo);
  }
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirect(Serialiser *localSerialiser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkBuffer buffer, VkDeviceSize offset,
                                                       uint32_t count, uint32_t stride)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
  SERIALISE_ELEMENT(uint64_t, offs, offset);

  SERIALISE_ELEMENT(uint32_t, cnt, count);
  SERIALISE_ELEMENT(uint32_t, strd, stride);

  bool multidraw = cnt > 1;

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  // do execution (possibly partial)
  if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    ObjDisp(commandBuffer)->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
  }
  else if(m_State == EXECUTING && cnt <= 1)
  {
    // for single draws, it's pretty simple
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid) && IsDrawInRenderPass())
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer);

      ObjDisp(commandBuffer)
          ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);

      if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
        m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
      }
    }
  }
  else if(m_State == EXECUTING)
  {
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

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
        for(uint32_t i = 0; i < cnt; i++)
        {
          uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, i + 1);

          ObjDisp(commandBuffer)
              ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, 1, strd);

          if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, 1, strd);
            m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
          }

          offs += strd;
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
          cnt = RDCMIN(cnt, m_LastEventID - baseEventID);
        }
        else
        {
          if(curEID > baseEventID)
          {
            // otherwise we do the 'hard' case, draw only one multidraw
            // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
            // a single draw.
            drawidx = (curEID - baseEventID - 1);

            RDCASSERT(drawidx < cnt, drawidx, cnt);

            if(drawidx >= cnt)
              drawidx = 0;

            offs += strd * drawidx;
            cnt = 1;
          }
          else
          {
            cnt = 0;
          }
        }

        if(IsDrawInRenderPass())
        {
          uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Drawcall, drawidx + 1);

          ObjDisp(commandBuffer)
              ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);

          if(eventID && m_DrawcallCallback->PostDraw(eventID, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
            m_DrawcallCallback->PostRedraw(eventID, commandBuffer);
          }
        }
      }
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages(localSerialiser, true);

  // while reading, create the drawcall set
  if(m_State == READING)
  {
    vector<byte> argbuf;
    GetDebugManager()->GetBufferData(
        GetResID(buffer), offs, sizeof(VkDrawIndexedIndirectCommand) + (cnt - 1) * strd, argbuf);

    string name = "vkCmdDrawIndexedIndirect(" + ToStr::Get(cnt) + ")";

    if(!IsDrawInRenderPass())
    {
      AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                      MessageSource::IncorrectAPIUse,
                      "Drawcall in happening outside of render pass, or in secondary command "
                      "buffer without RENDER_PASS_CONTINUE_BIT");
    }

    // for 'single' draws, don't do complex multi-draw just inline it
    if(cnt <= 1)
    {
      DrawcallDescription draw;

      if(cnt == 1)
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

      AddEvent(desc);

      draw.name = name;
      draw.flags = DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced;

      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(std::make_pair(
          GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

      return true;
    }

    DrawcallDescription draw;
    draw.name = name;
    draw.flags = DrawFlags::MultiDraw | DrawFlags::PushMarker;
    AddEvent(desc);
    AddDrawcall(draw, true);

    VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

    drawNode.resourceUsage.push_back(std::make_pair(
        GetResID(buffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

    uint32_t cmdOffs = 0;

    for(uint32_t i = 0; i < cnt; i++)
    {
      VkDrawIndexedIndirectCommand params = {};
      bool valid = false;

      if(cmdOffs + sizeof(VkDrawIndexedIndirectCommand) <= argbuf.size())
      {
        params = *((VkDrawIndexedIndirectCommand *)&argbuf[cmdOffs]);
        valid = true;
        cmdOffs += sizeof(VkDrawIndexedIndirectCommand);
      }

      offs += strd;

      DrawcallDescription multi;
      multi.numIndices = params.indexCount;
      multi.numInstances = params.instanceCount;
      multi.vertexOffset = params.vertexOffset;
      multi.indexOffset = params.firstIndex;
      multi.instanceOffset = params.firstInstance;

      multi.name = "vkCmdDrawIndexedIndirect[" + ToStr::Get(i) + "](<" +
                   ToStr::Get(multi.numIndices) + ", " + ToStr::Get(multi.numInstances) + ">)";

      multi.flags |=
          DrawFlags::Drawcall | DrawFlags::UseIBuffer | DrawFlags::Instanced | DrawFlags::Indirect;

      AddEvent(multi.name.elems);
      AddDrawcall(multi, true);

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
    }

    draw.name = name;
    draw.flags = DrawFlags::PopMarker;
    AddDrawcall(draw, false);
  }
  else if(multidraw)
  {
    // multidraws skip the event ID past the whole thing
    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID += cnt + 1;
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                             VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)
      ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INDIRECT);
    Serialise_vkCmdDrawIndexedIndirect(localSerialiser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
    if(GetRecord(buffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(buffer)->sparseInfo);
  }
}

bool WrappedVulkan::Serialise_vkCmdDispatch(Serialiser *localSerialiser,
                                            VkCommandBuffer commandBuffer, uint32_t x, uint32_t y,
                                            uint32_t z)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(uint32_t, X, x);
  SERIALISE_ELEMENT(uint32_t, Y, y);
  SERIALISE_ELEMENT(uint32_t, Z, z);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

      ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), X, Y, Z);

      if(eventID && m_DrawcallCallback->PostDispatch(eventID, commandBuffer))
      {
        ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), X, Y, Z);
        m_DrawcallCallback->PostRedispatch(eventID, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), X, Y, Z);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name =
          "vkCmdDispatch(" + ToStr::Get(X) + "," + ToStr::Get(Y) + "," + ToStr::Get(Z) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.dispatchDimension[0] = X;
      draw.dispatchDimension[1] = Y;
      draw.dispatchDimension[2] = Z;

      draw.flags |= DrawFlags::Dispatch;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DISPATCH);
    Serialise_vkCmdDispatch(localSerialiser, commandBuffer, x, y, z);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdDispatchIndirect(Serialiser *localSerialiser,
                                                    VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                    VkDeviceSize offset)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
  SERIALISE_ELEMENT(uint64_t, offs, offset);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Dispatch);

      ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs);

      if(eventID && m_DrawcallCallback->PostDispatch(eventID, commandBuffer))
      {
        ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs);
        m_DrawcallCallback->PostRedispatch(eventID, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs);

    const string desc = localSerialiser->GetDebugStr();

    {
      VkDispatchIndirectCommand unknown = {0};
      vector<byte> argbuf;
      GetDebugManager()->GetBufferData(GetResID(buffer), offs, sizeof(VkDispatchIndirectCommand),
                                       argbuf);
      VkDispatchIndirectCommand *args = (VkDispatchIndirectCommand *)&argbuf[0];

      if(argbuf.size() < sizeof(VkDispatchIndirectCommand))
      {
        RDCERR("Couldn't fetch arguments buffer for vkCmdDispatchIndirect");
        args = &unknown;
      }

      AddEvent(desc);
      string name = "vkCmdDispatchIndirect(<" + ToStr::Get(args->x) + "," + ToStr::Get(args->y) +
                    "," + ToStr::Get(args->z) + ">)";

      DrawcallDescription draw;
      draw.name = name;
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

  return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                          VkDeviceSize offset)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DISPATCH_INDIRECT);
    Serialise_vkCmdDispatchIndirect(localSerialiser, commandBuffer, buffer, offset);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
    if(GetRecord(buffer)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(buffer)->sparseInfo);
  }
}

bool WrappedVulkan::Serialise_vkCmdBlitImage(Serialiser *localSerialiser,
                                             VkCommandBuffer commandBuffer, VkImage srcImage,
                                             VkImageLayout srcImageLayout, VkImage destImage,
                                             VkImageLayout destImageLayout, uint32_t regionCount,
                                             const VkImageBlit *pRegions, VkFilter filter)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
  SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
  SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
  SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

  SERIALISE_ELEMENT(VkFilter, f, filter);

  SERIALISE_ELEMENT(uint32_t, count, regionCount);
  SERIALISE_ELEMENT_ARR(VkImageBlit, regions, pRegions, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Resolve);

      ObjDisp(commandBuffer)
          ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                         dstlayout, count, regions, f);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Resolve, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                           dstlayout, count, regions, f);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Resolve, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

    ObjDisp(commandBuffer)
        ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                       dstlayout, count, regions, f);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdBlitImage(" + ToStr::Get(srcid) + "," + ToStr::Get(dstid) + ")";

      DrawcallDescription draw;
      draw.name = name;
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

  SAFE_DELETE_ARRAY(regions);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(BLIT_IMG);
    Serialise_vkCmdBlitImage(localSerialiser, commandBuffer, srcImage, srcImageLayout, destImage,
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

bool WrappedVulkan::Serialise_vkCmdResolveImage(Serialiser *localSerialiser,
                                                VkCommandBuffer commandBuffer, VkImage srcImage,
                                                VkImageLayout srcImageLayout, VkImage destImage,
                                                VkImageLayout destImageLayout, uint32_t regionCount,
                                                const VkImageResolve *pRegions)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
  SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
  SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
  SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

  SERIALISE_ELEMENT(uint32_t, count, regionCount);
  SERIALISE_ELEMENT_ARR(VkImageResolve, regions, pRegions, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Resolve);

      ObjDisp(commandBuffer)
          ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                            dstlayout, count, regions);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Resolve, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                              dstlayout, count, regions);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Resolve, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

    ObjDisp(commandBuffer)
        ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                          dstlayout, count, regions);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdResolveImage(" + ToStr::Get(srcid) + "," + ToStr::Get(dstid) + ")";

      DrawcallDescription draw;
      draw.name = name;
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

  SAFE_DELETE_ARRAY(regions);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(RESOLVE_IMG);
    Serialise_vkCmdResolveImage(localSerialiser, commandBuffer, srcImage, srcImageLayout, destImage,
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

bool WrappedVulkan::Serialise_vkCmdCopyImage(Serialiser *localSerialiser,
                                             VkCommandBuffer commandBuffer, VkImage srcImage,
                                             VkImageLayout srcImageLayout, VkImage destImage,
                                             VkImageLayout destImageLayout, uint32_t regionCount,
                                             const VkImageCopy *pRegions)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
  SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
  SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
  SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

  SERIALISE_ELEMENT(uint32_t, count, regionCount);
  SERIALISE_ELEMENT_ARR(VkImageCopy, regions, pRegions, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

      ObjDisp(commandBuffer)
          ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                         dstlayout, count, regions);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                           dstlayout, count, regions);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

    ObjDisp(commandBuffer)
        ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage),
                       dstlayout, count, regions);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdCopyImage(" + ToStr::Get(srcid) + "," + ToStr::Get(dstid) + ")";

      DrawcallDescription draw;
      draw.name = name;
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

  SAFE_DELETE_ARRAY(regions);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(COPY_IMG);
    Serialise_vkCmdCopyImage(localSerialiser, commandBuffer, srcImage, srcImageLayout, destImage,
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

bool WrappedVulkan::Serialise_vkCmdCopyBufferToImage(Serialiser *localSerialiser,
                                                     VkCommandBuffer commandBuffer,
                                                     VkBuffer srcBuffer, VkImage destImage,
                                                     VkImageLayout destImageLayout,
                                                     uint32_t regionCount,
                                                     const VkBufferImageCopy *pRegions)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, bufid, GetResID(srcBuffer));
  SERIALISE_ELEMENT(ResourceId, imgid, GetResID(destImage));

  SERIALISE_ELEMENT(VkImageLayout, layout, destImageLayout);

  SERIALISE_ELEMENT(uint32_t, count, regionCount);
  SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

      ObjDisp(commandBuffer)
          ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                 layout, count, regions);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                   layout, count, regions);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
    destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

    ObjDisp(commandBuffer)
        ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage), layout,
                               count, regions);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdCopyBufferToImage(" + ToStr::Get(bufid) + "," + ToStr::Get(imgid) + ")";

      DrawcallDescription draw;
      draw.name = name;
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

  SAFE_DELETE_ARRAY(regions);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(COPY_BUF2IMG);
    Serialise_vkCmdCopyBufferToImage(localSerialiser, commandBuffer, srcBuffer, destImage,
                                     destImageLayout, regionCount, pRegions);

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

bool WrappedVulkan::Serialise_vkCmdCopyImageToBuffer(Serialiser *localSerialiser,
                                                     VkCommandBuffer commandBuffer,
                                                     VkImage srcImage, VkImageLayout srcImageLayout,
                                                     VkBuffer destBuffer, uint32_t regionCount,
                                                     const VkBufferImageCopy *pRegions)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
  SERIALISE_ELEMENT(ResourceId, imgid, GetResID(srcImage));

  SERIALISE_ELEMENT(VkImageLayout, layout, srcImageLayout);

  SERIALISE_ELEMENT(uint32_t, count, regionCount);
  SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
    destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

      ObjDisp(commandBuffer)
          ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), layout,
                                 Unwrap(destBuffer), count, regions);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), layout,
                                   Unwrap(destBuffer), count, regions);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
    destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

    ObjDisp(commandBuffer)
        ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer),
                               count, regions);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdCopyImageToBuffer(" + ToStr::Get(imgid) + "," + ToStr::Get(bufid) + ")";

      DrawcallDescription draw;
      draw.name = name;
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

  SAFE_DELETE_ARRAY(regions);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(COPY_IMG2BUF);
    Serialise_vkCmdCopyImageToBuffer(localSerialiser, commandBuffer, srcImage, srcImageLayout,
                                     destBuffer, regionCount, pRegions);

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

bool WrappedVulkan::Serialise_vkCmdCopyBuffer(Serialiser *localSerialiser,
                                              VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                                              VkBuffer destBuffer, uint32_t regionCount,
                                              const VkBufferCopy *pRegions)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcBuffer));
  SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destBuffer));

  SERIALISE_ELEMENT(uint32_t, count, regionCount);
  SERIALISE_ELEMENT_ARR(VkBufferCopy, regions, pRegions, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
    destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags::Copy);

      ObjDisp(commandBuffer)
          ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count,
                          regions);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags::Copy, commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count,
                            regions);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags::Copy, commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
    destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

    ObjDisp(commandBuffer)
        ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdCopyBuffer(" + ToStr::Get(srcid) + "," + ToStr::Get(dstid) + ")";

      DrawcallDescription draw;
      draw.name = name;
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

  SAFE_DELETE_ARRAY(regions);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(COPY_BUF);
    Serialise_vkCmdCopyBuffer(localSerialiser, commandBuffer, srcBuffer, destBuffer, regionCount,
                              pRegions);

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

bool WrappedVulkan::Serialise_vkCmdClearColorImage(Serialiser *localSerialiser,
                                                   VkCommandBuffer commandBuffer, VkImage image,
                                                   VkImageLayout imageLayout,
                                                   const VkClearColorValue *pColor,
                                                   uint32_t rangeCount,
                                                   const VkImageSubresourceRange *pRanges)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
  SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
  SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

  SERIALISE_ELEMENT(uint32_t, count, rangeCount);
  SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID =
          HandlePreCallback(commandBuffer, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor));

      ObjDisp(commandBuffer)
          ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), layout, &col, count, ranges);

      if(eventID &&
         m_DrawcallCallback->PostMisc(eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor),
                                      commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), layout, &col, count, ranges);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearColor),
                                       commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

    ObjDisp(commandBuffer)
        ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), layout, &col, count, ranges);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdClearColorImage(" + ToStr::Get(col) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;
      draw.copyDestination = imgid;

      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(
          std::make_pair(GetResID(image), EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
    }
  }

  SAFE_DELETE_ARRAY(ranges);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
    Serialise_vkCmdClearColorImage(localSerialiser, commandBuffer, image, imageLayout, pColor,
                                   rangeCount, pRanges);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    if(GetRecord(image)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(image)->sparseInfo);
  }
}

bool WrappedVulkan::Serialise_vkCmdClearDepthStencilImage(
    Serialiser *localSerialiser, VkCommandBuffer commandBuffer, VkImage image,
    VkImageLayout imageLayout, const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount,
    const VkImageSubresourceRange *pRanges)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
  SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
  SERIALISE_ELEMENT(VkImageLayout, l, imageLayout);
  SERIALISE_ELEMENT(VkClearDepthStencilValue, ds, *pDepthStencil);
  SERIALISE_ELEMENT(uint32_t, count, rangeCount);
  SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(
          commandBuffer, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil));

      ObjDisp(commandBuffer)
          ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), l, &ds, count, ranges);

      if(eventID &&
         m_DrawcallCallback->PostMisc(
             eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil), commandBuffer))
      {
        ObjDisp(commandBuffer)
            ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), l, &ds, count, ranges);

        m_DrawcallCallback->PostRemisc(
            eventID, DrawFlags(DrawFlags::Clear | DrawFlags::ClearDepthStencil), commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

    ObjDisp(commandBuffer)
        ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), l, &ds, count, ranges);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name =
          "vkCmdClearDepthStencilImage(" + ToStr::Get(ds.depth) + "," + ToStr::Get(ds.stencil) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;
      draw.copyDestination = imgid;

      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(
          std::make_pair(GetResID(image), EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
    }
  }

  SAFE_DELETE_ARRAY(ranges);

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

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL);
    Serialise_vkCmdClearDepthStencilImage(localSerialiser, commandBuffer, image, imageLayout,
                                          pDepthStencil, rangeCount, pRanges);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    if(GetRecord(image)->sparseInfo)
      record->cmdInfo->sparse.insert(GetRecord(image)->sparseInfo);
  }
}

bool WrappedVulkan::Serialise_vkCmdClearAttachments(Serialiser *localSerialiser,
                                                    VkCommandBuffer commandBuffer,
                                                    uint32_t attachmentCount,
                                                    const VkClearAttachment *pAttachments,
                                                    uint32_t rectCount, const VkClearRect *pRects)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));

  SERIALISE_ELEMENT(uint32_t, acount, attachmentCount);
  SERIALISE_ELEMENT_ARR(VkClearAttachment, atts, pAttachments, acount);

  SERIALISE_ELEMENT(uint32_t, rcount, rectCount);
  SERIALISE_ELEMENT_ARR(VkClearRect, rects, pRects, rcount);

  Serialise_DebugMessages(localSerialiser, true);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      commandBuffer = RerecordCmdBuf(cmdid);

      uint32_t eventID = HandlePreCallback(commandBuffer, DrawFlags(DrawFlags::Clear));

      ObjDisp(commandBuffer)->CmdClearAttachments(Unwrap(commandBuffer), acount, atts, rcount, rects);

      if(eventID && m_DrawcallCallback->PostMisc(eventID, DrawFlags(DrawFlags::Clear), commandBuffer))
      {
        ObjDisp(commandBuffer)->CmdClearAttachments(Unwrap(commandBuffer), acount, atts, rcount, rects);

        m_DrawcallCallback->PostRemisc(eventID, DrawFlags(DrawFlags::Clear), commandBuffer);
      }
    }
  }
  else if(m_State == READING)
  {
    commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(commandBuffer)->CmdClearAttachments(Unwrap(commandBuffer), acount, atts, rcount, rects);

    const string desc = localSerialiser->GetDebugStr();

    {
      AddEvent(desc);
      string name = "vkCmdClearAttachments(";
      for(uint32_t a = 0; a < acount; a++)
        name += ToStr::Get(atts[a]);
      name += ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear;
      for(uint32_t a = 0; a < acount; a++)
      {
        if(atts[a].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
          draw.flags |= DrawFlags::ClearColor;
        if(atts[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
          draw.flags |= DrawFlags::ClearDepthStencil;
      }

      AddDrawcall(draw, true);

      VulkanDrawcallTreeNode &drawNode = GetDrawcallStack().back()->children.back();
      const BakedCmdBufferInfo::CmdBufferState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

      if(state.renderPass != ResourceId() && state.framebuffer != ResourceId())
      {
        VulkanCreationInfo::RenderPass &rp = m_CreationInfo.m_RenderPass[state.renderPass];
        VulkanCreationInfo::Framebuffer &fb = m_CreationInfo.m_Framebuffer[state.framebuffer];

        RDCASSERT(state.subpass < rp.subpasses.size());

        for(size_t i = 0; i < rp.subpasses[state.subpass].colorAttachments.size(); i++)
        {
          uint32_t att = rp.subpasses[state.subpass].colorAttachments[i];
          drawNode.resourceUsage.push_back(std::make_pair(
              m_CreationInfo.m_ImageView[fb.attachments[att].view].image,
              EventUsage(drawNode.draw.eventID, ResourceUsage::Clear, fb.attachments[att].view)));
        }

        if(draw.flags & DrawFlags::ClearDepthStencil &&
           rp.subpasses[state.subpass].depthstencilAttachment >= 0)
        {
          int32_t att = rp.subpasses[state.subpass].depthstencilAttachment;
          drawNode.resourceUsage.push_back(std::make_pair(
              m_CreationInfo.m_ImageView[fb.attachments[att].view].image,
              EventUsage(drawNode.draw.eventID, ResourceUsage::Clear, fb.attachments[att].view)));
        }
      }
    }
  }

  SAFE_DELETE_ARRAY(atts);
  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedVulkan::vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount,
                                          const VkClearAttachment *pAttachments, uint32_t rectCount,
                                          const VkClearRect *pRects)
{
  SCOPED_DBG_SINK();

  ObjDisp(commandBuffer)
      ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount, pRects);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(CLEAR_ATTACH);
    Serialise_vkCmdClearAttachments(localSerialiser, commandBuffer, attachmentCount, pAttachments,
                                    rectCount, pRects);

    record->AddChunk(scope.Get());

    // image/attachments are referenced when the render pass is started and the framebuffer is
    // bound.
  }
}
