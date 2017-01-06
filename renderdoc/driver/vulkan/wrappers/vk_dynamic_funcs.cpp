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

bool WrappedVulkan::Serialise_vkCmdSetViewport(Serialiser *localSerialiser,
                                               VkCommandBuffer cmdBuffer, uint32_t firstViewport,
                                               uint32_t viewportCount, const VkViewport *pViewports)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(uint32_t, first, firstViewport);
  SERIALISE_ELEMENT(uint32_t, count, viewportCount);
  SERIALISE_ELEMENT_ARR(VkViewport, views, pViewports, count);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetViewport(Unwrap(cmdBuffer), first, count, views);

      if(m_RenderState.views.size() < first + count)
        m_RenderState.views.resize(first + count);

      for(uint32_t i = 0; i < count; i++)
        m_RenderState.views[first + i] = views[i];
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetViewport(Unwrap(cmdBuffer), first, count, views);
  }

  SAFE_DELETE_ARRAY(views);

  return true;
}

void WrappedVulkan::vkCmdSetViewport(VkCommandBuffer cmdBuffer, uint32_t firstViewport,
                                     uint32_t viewportCount, const VkViewport *pViewports)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetViewport(Unwrap(cmdBuffer), firstViewport, viewportCount, pViewports);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_VP);
    Serialise_vkCmdSetViewport(localSerialiser, cmdBuffer, firstViewport, viewportCount, pViewports);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetScissor(Serialiser *localSerialiser,
                                              VkCommandBuffer cmdBuffer, uint32_t firstScissor,
                                              uint32_t scissorCount, const VkRect2D *pScissors)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(uint32_t, first, firstScissor);
  SERIALISE_ELEMENT(uint32_t, count, scissorCount);
  SERIALISE_ELEMENT_ARR(VkRect2D, scissors, pScissors, count);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetScissor(Unwrap(cmdBuffer), first, count, scissors);

      if(m_RenderState.scissors.size() < first + count)
        m_RenderState.scissors.resize(first + count);

      for(uint32_t i = 0; i < count; i++)
        m_RenderState.scissors[first + i] = scissors[i];
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetScissor(Unwrap(cmdBuffer), first, count, scissors);
  }

  SAFE_DELETE_ARRAY(scissors);

  return true;
}

void WrappedVulkan::vkCmdSetScissor(VkCommandBuffer cmdBuffer, uint32_t firstScissor,
                                    uint32_t scissorCount, const VkRect2D *pScissors)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetScissor(Unwrap(cmdBuffer), firstScissor, scissorCount, pScissors);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_SCISSOR);
    Serialise_vkCmdSetScissor(localSerialiser, cmdBuffer, firstScissor, scissorCount, pScissors);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetLineWidth(Serialiser *localSerialiser,
                                                VkCommandBuffer cmdBuffer, float lineWidth)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(float, width, lineWidth);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetLineWidth(Unwrap(cmdBuffer), width);
      m_RenderState.lineWidth = width;
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetLineWidth(Unwrap(cmdBuffer), width);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLineWidth(VkCommandBuffer cmdBuffer, float lineWidth)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetLineWidth(Unwrap(cmdBuffer), lineWidth);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_LINE_WIDTH);
    Serialise_vkCmdSetLineWidth(localSerialiser, cmdBuffer, lineWidth);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetDepthBias(Serialiser *localSerialiser,
                                                VkCommandBuffer cmdBuffer, float depthBias,
                                                float depthBiasClamp, float slopeScaledDepthBias)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(float, bias, depthBias);
  SERIALISE_ELEMENT(float, biasclamp, depthBiasClamp);
  SERIALISE_ELEMENT(float, slope, slopeScaledDepthBias);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetDepthBias(Unwrap(cmdBuffer), bias, biasclamp, slope);
      m_RenderState.bias.depth = bias;
      m_RenderState.bias.biasclamp = biasclamp;
      m_RenderState.bias.slope = slope;
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetDepthBias(Unwrap(cmdBuffer), bias, biasclamp, slope);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthBias(VkCommandBuffer cmdBuffer, float depthBias,
                                      float depthBiasClamp, float slopeScaledDepthBias)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetDepthBias(Unwrap(cmdBuffer), depthBias, depthBiasClamp,
                                      slopeScaledDepthBias);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_DEPTH_BIAS);
    Serialise_vkCmdSetDepthBias(localSerialiser, cmdBuffer, depthBias, depthBiasClamp,
                                slopeScaledDepthBias);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetBlendConstants(Serialiser *localSerialiser,
                                                     VkCommandBuffer cmdBuffer,
                                                     const float *blendConst)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));

  float blendFactor[4];
  if(m_State >= WRITING)
  {
    blendFactor[0] = blendConst[0];
    blendFactor[1] = blendConst[1];
    blendFactor[2] = blendConst[2];
    blendFactor[3] = blendConst[3];
  }
  localSerialiser->SerialisePODArray<4>("blendConst", blendFactor);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetBlendConstants(Unwrap(cmdBuffer), blendFactor);
      memcpy(m_RenderState.blendConst, blendFactor, sizeof(blendFactor));
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetBlendConstants(Unwrap(cmdBuffer), blendFactor);
  }

  return true;
}

void WrappedVulkan::vkCmdSetBlendConstants(VkCommandBuffer cmdBuffer, const float *blendConst)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetBlendConstants(Unwrap(cmdBuffer), blendConst);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_BLEND_CONST);
    Serialise_vkCmdSetBlendConstants(localSerialiser, cmdBuffer, blendConst);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetDepthBounds(Serialiser *localSerialiser,
                                                  VkCommandBuffer cmdBuffer, float minDepthBounds,
                                                  float maxDepthBounds)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(float, mind, minDepthBounds);
  SERIALISE_ELEMENT(float, maxd, maxDepthBounds);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetDepthBounds(Unwrap(cmdBuffer), mind, maxd);
      m_RenderState.mindepth = mind;
      m_RenderState.maxdepth = maxd;
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetDepthBounds(Unwrap(cmdBuffer), mind, maxd);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthBounds(VkCommandBuffer cmdBuffer, float minDepthBounds,
                                        float maxDepthBounds)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetDepthBounds(Unwrap(cmdBuffer), minDepthBounds, maxDepthBounds);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_DEPTH_BOUNDS);
    Serialise_vkCmdSetDepthBounds(localSerialiser, cmdBuffer, minDepthBounds, maxDepthBounds);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetStencilCompareMask(Serialiser *localSerialiser,
                                                         VkCommandBuffer cmdBuffer,
                                                         VkStencilFaceFlags faceMask,
                                                         uint32_t compareMask)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(VkStencilFaceFlagBits, face, (VkStencilFaceFlagBits)faceMask);
  SERIALISE_ELEMENT(uint32_t, mask, compareMask);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetStencilCompareMask(Unwrap(cmdBuffer), face, mask);

      if(face & VK_STENCIL_FACE_FRONT_BIT)
        m_RenderState.front.compare = mask;
      if(face & VK_STENCIL_FACE_BACK_BIT)
        m_RenderState.back.compare = mask;
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetStencilCompareMask(Unwrap(cmdBuffer), face, mask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilCompareMask(VkCommandBuffer cmdBuffer,
                                               VkStencilFaceFlags faceMask, uint32_t compareMask)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetStencilCompareMask(Unwrap(cmdBuffer), faceMask, compareMask);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_STENCIL_COMP_MASK);
    Serialise_vkCmdSetStencilCompareMask(localSerialiser, cmdBuffer, faceMask, compareMask);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetStencilWriteMask(Serialiser *localSerialiser,
                                                       VkCommandBuffer cmdBuffer,
                                                       VkStencilFaceFlags faceMask,
                                                       uint32_t writeMask)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(VkStencilFaceFlagBits, face, (VkStencilFaceFlagBits)faceMask);
  SERIALISE_ELEMENT(uint32_t, mask, writeMask);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetStencilWriteMask(Unwrap(cmdBuffer), face, mask);

      if(face & VK_STENCIL_FACE_FRONT_BIT)
        m_RenderState.front.write = mask;
      if(face & VK_STENCIL_FACE_BACK_BIT)
        m_RenderState.back.write = mask;
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetStencilWriteMask(Unwrap(cmdBuffer), face, mask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilWriteMask(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask,
                                             uint32_t writeMask)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetStencilWriteMask(Unwrap(cmdBuffer), faceMask, writeMask);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_STENCIL_WRITE_MASK);
    Serialise_vkCmdSetStencilWriteMask(localSerialiser, cmdBuffer, faceMask, writeMask);

    record->AddChunk(scope.Get());
  }
}

bool WrappedVulkan::Serialise_vkCmdSetStencilReference(Serialiser *localSerialiser,
                                                       VkCommandBuffer cmdBuffer,
                                                       VkStencilFaceFlags faceMask,
                                                       uint32_t reference)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(VkStencilFaceFlagBits, face, (VkStencilFaceFlagBits)faceMask);
  SERIALISE_ELEMENT(uint32_t, mask, reference);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetStencilReference(Unwrap(cmdBuffer), face, mask);

      if(face & VK_STENCIL_FACE_FRONT_BIT)
        m_RenderState.front.ref = mask;
      if(face & VK_STENCIL_FACE_BACK_BIT)
        m_RenderState.back.ref = mask;
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    ObjDisp(cmdBuffer)->CmdSetStencilReference(Unwrap(cmdBuffer), face, mask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilReference(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask,
                                             uint32_t reference)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetStencilReference(Unwrap(cmdBuffer), faceMask, reference);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_STENCIL_REF);
    Serialise_vkCmdSetStencilReference(localSerialiser, cmdBuffer, faceMask, reference);

    record->AddChunk(scope.Get());
  }
}
