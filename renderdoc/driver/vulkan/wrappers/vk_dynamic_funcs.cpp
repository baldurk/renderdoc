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

#include "../vk_core.h"

#define ILLEGAL_EDS3_CALL(func) RDCERR("Illegal function call: " #func);

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetViewport(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               uint32_t firstViewport, uint32_t viewportCount,
                                               const VkViewport *pViewports)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstViewport).Important();
  SERIALISE_ELEMENT(viewportCount);
  SERIALISE_ELEMENT_ARRAY(pViewports, viewportCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicViewport] = true;

          if(renderstate.views.size() < firstViewport + viewportCount)
            renderstate.views.resize(firstViewport + viewportCount);

          for(uint32_t i = 0; i < viewportCount; i++)
            renderstate.views[firstViewport + i] = pViewports[i];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetViewport(Unwrap(commandBuffer), firstViewport, viewportCount, pViewports);
  }

  return true;
}

void WrappedVulkan::vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                                     uint32_t viewportCount, const VkViewport *pViewports)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetViewport(Unwrap(commandBuffer), firstViewport, viewportCount, pViewports));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetViewport);
    Serialise_vkCmdSetViewport(ser, commandBuffer, firstViewport, viewportCount, pViewports);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetViewportWithCount(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        uint32_t viewportCount,
                                                        const VkViewport *pViewports)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(viewportCount);
  SERIALISE_ELEMENT_ARRAY(pViewports, viewportCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicViewportCount] = true;

          renderstate.views.assign(pViewports, viewportCount);
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetViewportWithCount(Unwrap(commandBuffer), viewportCount, pViewports);
  }

  return true;
}

void WrappedVulkan::vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount,
                                              const VkViewport *pViewports)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetViewportWithCount(Unwrap(commandBuffer), viewportCount, pViewports));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetViewportWithCount);
    Serialise_vkCmdSetViewportWithCount(ser, commandBuffer, viewportCount, pViewports);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetScissor(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              uint32_t firstScissor, uint32_t scissorCount,
                                              const VkRect2D *pScissors)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstScissor).Important();
  SERIALISE_ELEMENT(scissorCount);
  SERIALISE_ELEMENT_ARRAY(pScissors, scissorCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicScissor] = true;

          if(renderstate.scissors.size() < firstScissor + scissorCount)
            renderstate.scissors.resize(firstScissor + scissorCount);

          for(uint32_t i = 0; i < scissorCount; i++)
            renderstate.scissors[firstScissor + i] = pScissors[i];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetScissor(Unwrap(commandBuffer), firstScissor, scissorCount, pScissors);
  }

  return true;
}

void WrappedVulkan::vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                                    uint32_t scissorCount, const VkRect2D *pScissors)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetScissor(Unwrap(commandBuffer), firstScissor, scissorCount, pScissors));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetScissor);
    Serialise_vkCmdSetScissor(ser, commandBuffer, firstScissor, scissorCount, pScissors);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetScissorWithCount(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       uint32_t scissorCount,
                                                       const VkRect2D *pScissors)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(scissorCount);
  SERIALISE_ELEMENT_ARRAY(pScissors, scissorCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicScissorCount] = true;

          renderstate.scissors.assign(pScissors, scissorCount);
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetScissorWithCount(Unwrap(commandBuffer), scissorCount, pScissors);
  }

  return true;
}

void WrappedVulkan::vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount,
                                             const VkRect2D *pScissors)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetScissorWithCount(Unwrap(commandBuffer), scissorCount, pScissors));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetScissorWithCount);
    Serialise_vkCmdSetScissorWithCount(ser, commandBuffer, scissorCount, pScissors);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLineWidth(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                float lineWidth)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(lineWidth).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicLineWidth] = true;
          renderstate.lineWidth = lineWidth;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetLineWidth(Unwrap(commandBuffer), lineWidth);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdSetLineWidth(Unwrap(commandBuffer), lineWidth));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLineWidth);
    Serialise_vkCmdSetLineWidth(ser, commandBuffer, lineWidth);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthBias(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                float depthBias, float depthBiasClamp,
                                                float slopeScaledDepthBias)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthBias).Important();
  SERIALISE_ELEMENT(depthBiasClamp).Important();
  SERIALISE_ELEMENT(slopeScaledDepthBias).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthBias] = true;

          renderstate.bias.depth = depthBias;
          renderstate.bias.biasclamp = depthBiasClamp;
          renderstate.bias.slope = slopeScaledDepthBias;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetDepthBias(Unwrap(commandBuffer), depthBias, depthBiasClamp, slopeScaledDepthBias);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBias,
                                      float depthBiasClamp, float slopeScaledDepthBias)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetDepthBias(Unwrap(commandBuffer), depthBias, depthBiasClamp,
                                            slopeScaledDepthBias));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthBias);
    Serialise_vkCmdSetDepthBias(ser, commandBuffer, depthBias, depthBiasClamp, slopeScaledDepthBias);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetBlendConstants(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     const float *blendConst)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_ARRAY(blendConst, 4).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicBlendConstants] = true;

          memcpy(renderstate.blendConst, blendConst, sizeof(renderstate.blendConst));
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetBlendConstants(Unwrap(commandBuffer), blendConst);
  }

  return true;
}

void WrappedVulkan::vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float *blendConst)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdSetBlendConstants(Unwrap(commandBuffer), blendConst));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetBlendConstants);
    Serialise_vkCmdSetBlendConstants(ser, commandBuffer, blendConst);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthBounds(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  float minDepthBounds, float maxDepthBounds)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(minDepthBounds).Important();
  SERIALISE_ELEMENT(maxDepthBounds).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthBounds] = true;

          renderstate.mindepth = minDepthBounds;
          renderstate.maxdepth = maxDepthBounds;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthBounds(Unwrap(commandBuffer), minDepthBounds, maxDepthBounds);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds,
                                        float maxDepthBounds)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthBounds(Unwrap(commandBuffer), minDepthBounds, maxDepthBounds));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthBounds);
    Serialise_vkCmdSetDepthBounds(ser, commandBuffer, minDepthBounds, maxDepthBounds);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilCompareMask(SerialiserType &ser,
                                                         VkCommandBuffer commandBuffer,
                                                         VkStencilFaceFlags faceMask,
                                                         uint32_t compareMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).TypedAs("VkStencilFaceFlags"_lit).Important();
  SERIALISE_ELEMENT(compareMask).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicStencilCompareMask] = true;

          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
            renderstate.front.compare = compareMask;
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
            renderstate.back.compare = compareMask;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetStencilCompareMask(Unwrap(commandBuffer), faceMask, compareMask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer,
                                               VkStencilFaceFlags faceMask, uint32_t compareMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetStencilCompareMask(Unwrap(commandBuffer), faceMask, compareMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetStencilCompareMask);
    Serialise_vkCmdSetStencilCompareMask(ser, commandBuffer, faceMask, compareMask);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilWriteMask(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkStencilFaceFlags faceMask,
                                                       uint32_t writeMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).TypedAs("VkStencilFaceFlags"_lit).Important();
  SERIALISE_ELEMENT(writeMask).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicStencilWriteMask] = true;

          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
            renderstate.front.write = writeMask;
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
            renderstate.back.write = writeMask;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetStencilWriteMask(Unwrap(commandBuffer), faceMask, writeMask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer,
                                             VkStencilFaceFlags faceMask, uint32_t writeMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetStencilWriteMask(Unwrap(commandBuffer), faceMask, writeMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetStencilWriteMask);
    Serialise_vkCmdSetStencilWriteMask(ser, commandBuffer, faceMask, writeMask);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilReference(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkStencilFaceFlags faceMask,
                                                       uint32_t reference)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).TypedAs("VkStencilFaceFlags"_lit).Important();
  SERIALISE_ELEMENT(reference).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicStencilReference] = true;

          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
            renderstate.front.ref = reference;
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
            renderstate.back.ref = reference;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetStencilReference(Unwrap(commandBuffer), faceMask, reference);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilReference(VkCommandBuffer commandBuffer,
                                             VkStencilFaceFlags faceMask, uint32_t reference)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetStencilReference(Unwrap(commandBuffer), faceMask, reference));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetStencilReference);
    Serialise_vkCmdSetStencilReference(ser, commandBuffer, faceMask, reference);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetSampleLocationsEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkSampleLocationsInfoEXT *pSampleLocationsInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(sampleInfo, *pSampleLocationsInfo).Named("pSampleLocationsInfo"_lit).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicSampleLocationsEXT] = true;

          renderstate.sampleLocations.locations.assign(sampleInfo.pSampleLocations,
                                                       sampleInfo.sampleLocationsCount);
          renderstate.sampleLocations.gridSize = sampleInfo.sampleLocationGridSize;
          renderstate.sampleLocations.sampleCount = sampleInfo.sampleLocationsPerPixel;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetSampleLocationsEXT(Unwrap(commandBuffer), &sampleInfo);
  }

  return true;
}

void WrappedVulkan::vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer,
                                               const VkSampleLocationsInfoEXT *pSampleLocationsInfo)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetSampleLocationsEXT(Unwrap(commandBuffer), pSampleLocationsInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetSampleLocationsEXT);
    Serialise_vkCmdSetSampleLocationsEXT(ser, commandBuffer, pSampleLocationsInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDiscardRectangleEXT(SerialiserType &ser,
                                                          VkCommandBuffer commandBuffer,
                                                          uint32_t firstDiscardRectangle,
                                                          uint32_t discardRectangleCount,
                                                          const VkRect2D *pDiscardRectangles)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstDiscardRectangle).Important();
  SERIALISE_ELEMENT(discardRectangleCount);
  SERIALISE_ELEMENT_ARRAY(pDiscardRectangles, discardRectangleCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDiscardRectangleEXT] = true;

          if(renderstate.discardRectangles.size() < firstDiscardRectangle + discardRectangleCount)
            renderstate.discardRectangles.resize(firstDiscardRectangle + discardRectangleCount);

          for(uint32_t i = 0; i < discardRectangleCount; i++)
            renderstate.discardRectangles[firstDiscardRectangle + i] = pDiscardRectangles[i];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetDiscardRectangleEXT(Unwrap(commandBuffer), firstDiscardRectangle,
                                      discardRectangleCount, pDiscardRectangles);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDiscardRectangleEXT(VkCommandBuffer commandBuffer,
                                                uint32_t firstDiscardRectangle,
                                                uint32_t discardRectangleCount,
                                                const VkRect2D *pDiscardRectangles)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetDiscardRectangleEXT(Unwrap(commandBuffer), firstDiscardRectangle,
                                                      discardRectangleCount, pDiscardRectangles));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDiscardRectangleEXT);
    Serialise_vkCmdSetDiscardRectangleEXT(ser, commandBuffer, firstDiscardRectangle,
                                          discardRectangleCount, pDiscardRectangles);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLineStippleKHR(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     uint32_t lineStippleFactor,
                                                     uint16_t lineStipplePattern)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(lineStippleFactor).Important();
  SERIALISE_ELEMENT(lineStipplePattern).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicLineStippleKHR] = true;

          renderstate.stippleFactor = lineStippleFactor;
          renderstate.stipplePattern = lineStipplePattern;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    // this could be the EXT function, we handle either one in our dispatch tables and assign to the
    // other since it's a straight promotion. This allows us to share implementation between the two
    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetLineStippleKHR(Unwrap(commandBuffer), lineStippleFactor, lineStipplePattern);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLineStippleKHR(VkCommandBuffer commandBuffer,
                                           uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
  SCOPED_DBG_SINK();

  // this could be the EXT function, we handle either one in our dispatch tables and assign to the
  // other since it's a straight promotion. This allows us to share implementation between the two
  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetLineStippleKHR(Unwrap(commandBuffer), lineStippleFactor, lineStipplePattern));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLineStippleKHR);
    Serialise_vkCmdSetLineStippleKHR(ser, commandBuffer, lineStippleFactor, lineStipplePattern);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

// we can forward this straight on regardless, as our handling of dispatch tables means if only the
// EXT is available it will go through to the EXT from the driver, and we would rather merge the
// EXT/KHR paths and silently promote than keep them separate and maintain two paths.
void WrappedVulkan::vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer,
                                           uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
  return vkCmdSetLineStippleKHR(commandBuffer, lineStippleFactor, lineStipplePattern);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetCullMode(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               VkCullModeFlags cullMode)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkCullModeFlagBits, cullMode).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicCullMode] = true;

          renderstate.cullMode = cullMode;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetCullMode(Unwrap(commandBuffer), cullMode);
  }

  return true;
}

void WrappedVulkan::vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdSetCullMode(Unwrap(commandBuffer), cullMode));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetCullMode);
    Serialise_vkCmdSetCullMode(ser, commandBuffer, cullMode);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetFrontFace(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkFrontFace frontFace)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(frontFace).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicFrontFace] = true;

          renderstate.frontFace = frontFace;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetFrontFace(Unwrap(commandBuffer), frontFace);
  }

  return true;
}

void WrappedVulkan::vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdSetFrontFace(Unwrap(commandBuffer), frontFace));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetFrontFace);
    Serialise_vkCmdSetFrontFace(ser, commandBuffer, frontFace);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetPrimitiveTopology(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        VkPrimitiveTopology primitiveTopology)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(primitiveTopology).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicPrimitiveTopology] = true;

          renderstate.primitiveTopology = primitiveTopology;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }
    else
    {
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.primitiveTopology = primitiveTopology;
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetPrimitiveTopology(Unwrap(commandBuffer), primitiveTopology);
  }

  return true;
}

void WrappedVulkan::vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer,
                                              VkPrimitiveTopology primitiveTopology)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetPrimitiveTopology(Unwrap(commandBuffer), primitiveTopology));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetPrimitiveTopology);
    Serialise_vkCmdSetPrimitiveTopology(ser, commandBuffer, primitiveTopology);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthTestEnable(SerialiserType &ser,
                                                      VkCommandBuffer commandBuffer,
                                                      VkBool32 depthTestEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthTestEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthTestEnable] = true;

          renderstate.depthTestEnable = depthTestEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthTestEnable(Unwrap(commandBuffer), depthTestEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthTestEnable(Unwrap(commandBuffer), depthTestEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthTestEnable);
    Serialise_vkCmdSetDepthTestEnable(ser, commandBuffer, depthTestEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthWriteEnable(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkBool32 depthWriteEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthWriteEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthWriteEnable] = true;

          renderstate.depthWriteEnable = depthWriteEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthWriteEnable(Unwrap(commandBuffer), depthWriteEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthWriteEnable(Unwrap(commandBuffer), depthWriteEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthWriteEnable);
    Serialise_vkCmdSetDepthWriteEnable(ser, commandBuffer, depthWriteEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthCompareOp(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     VkCompareOp depthCompareOp)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthCompareOp).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthCompareOp] = true;

          renderstate.depthCompareOp = depthCompareOp;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthCompareOp(Unwrap(commandBuffer), depthCompareOp);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthCompareOp(Unwrap(commandBuffer), depthCompareOp));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthCompareOp);
    Serialise_vkCmdSetDepthCompareOp(ser, commandBuffer, depthCompareOp);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthBoundsTestEnable(SerialiserType &ser,
                                                            VkCommandBuffer commandBuffer,
                                                            VkBool32 depthBoundsTestEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthBoundsTestEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthBoundsTestEnable] = true;

          renderstate.depthBoundsTestEnable = depthBoundsTestEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthBoundsTestEnable(Unwrap(commandBuffer), depthBoundsTestEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer,
                                                  VkBool32 depthBoundsTestEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthBoundsTestEnable(Unwrap(commandBuffer), depthBoundsTestEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthBoundsTestEnable);
    Serialise_vkCmdSetDepthBoundsTestEnable(ser, commandBuffer, depthBoundsTestEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilTestEnable(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        VkBool32 stencilTestEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(stencilTestEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicStencilTestEnable] = true;

          renderstate.stencilTestEnable = stencilTestEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetStencilTestEnable(Unwrap(commandBuffer), stencilTestEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer,
                                              VkBool32 stencilTestEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetStencilTestEnable(Unwrap(commandBuffer), stencilTestEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetStencilTestEnable);
    Serialise_vkCmdSetStencilTestEnable(ser, commandBuffer, stencilTestEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilOp(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkStencilFaceFlags faceMask, VkStencilOp failOp,
                                                VkStencilOp passOp, VkStencilOp depthFailOp,
                                                VkCompareOp compareOp)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).Important();
  SERIALISE_ELEMENT(failOp);
  SERIALISE_ELEMENT(passOp);
  SERIALISE_ELEMENT(depthFailOp);
  SERIALISE_ELEMENT(compareOp).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicStencilOp] = true;

          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
          {
            renderstate.front.failOp = failOp;
            renderstate.front.passOp = passOp;
            renderstate.front.depthFailOp = depthFailOp;
            renderstate.front.compareOp = compareOp;
          }
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
          {
            renderstate.back.failOp = failOp;
            renderstate.back.passOp = passOp;
            renderstate.back.depthFailOp = depthFailOp;
            renderstate.back.compareOp = compareOp;
          }
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetStencilOp(Unwrap(commandBuffer), faceMask, failOp, passOp, depthFailOp, compareOp);
  }

  return true;
}

void WrappedVulkan::vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                      VkStencilOp failOp, VkStencilOp passOp,
                                      VkStencilOp depthFailOp, VkCompareOp compareOp)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetStencilOp(Unwrap(commandBuffer), faceMask, failOp, passOp,
                                            depthFailOp, compareOp));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetStencilOp);
    Serialise_vkCmdSetStencilOp(ser, commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetColorWriteEnableEXT(SerialiserType &ser,
                                                          VkCommandBuffer commandBuffer,
                                                          uint32_t attachmentCount,
                                                          const VkBool32 *pColorWriteEnables)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(attachmentCount);
  SERIALISE_ELEMENT_ARRAY(pColorWriteEnables, attachmentCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicColorWriteEXT] = true;

          renderstate.colorWriteEnable.assign(pColorWriteEnables, attachmentCount);
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetColorWriteEnableEXT(Unwrap(commandBuffer), attachmentCount, pColorWriteEnables);
  }

  return true;
}

void WrappedVulkan::vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer,
                                                uint32_t attachmentCount,
                                                const VkBool32 *pColorWriteEnables)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetColorWriteEnableEXT(Unwrap(commandBuffer), attachmentCount, pColorWriteEnables));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetColorWriteEnableEXT);
    Serialise_vkCmdSetColorWriteEnableEXT(ser, commandBuffer, attachmentCount, pColorWriteEnables);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthBiasEnable(SerialiserType &ser,
                                                      VkCommandBuffer commandBuffer,
                                                      VkBool32 depthBiasEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthBiasEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthBiasEnable] = true;

          renderstate.depthBiasEnable = depthBiasEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthBiasEnable(Unwrap(commandBuffer), depthBiasEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthBiasEnable(Unwrap(commandBuffer), depthBiasEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthBiasEnable);
    Serialise_vkCmdSetDepthBiasEnable(ser, commandBuffer, depthBiasEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLogicOpEXT(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                 VkLogicOp logicOp)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(logicOp).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicLogicOpEXT] = true;

          renderstate.logicOp = logicOp;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetLogicOpEXT(Unwrap(commandBuffer), logicOp);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdSetLogicOpEXT(Unwrap(commandBuffer), logicOp));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLogicOpEXT);
    Serialise_vkCmdSetLogicOpEXT(ser, commandBuffer, logicOp);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetPatchControlPointsEXT(SerialiserType &ser,
                                                            VkCommandBuffer commandBuffer,
                                                            uint32_t patchControlPoints)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(patchControlPoints).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicControlPointsEXT] = true;

          renderstate.patchControlPoints = patchControlPoints;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetPatchControlPointsEXT(Unwrap(commandBuffer), patchControlPoints);
  }

  return true;
}

void WrappedVulkan::vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer,
                                                  uint32_t patchControlPoints)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetPatchControlPointsEXT(Unwrap(commandBuffer), patchControlPoints));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetPatchControlPointsEXT);
    Serialise_vkCmdSetPatchControlPointsEXT(ser, commandBuffer, patchControlPoints);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetPrimitiveRestartEnable(SerialiserType &ser,
                                                             VkCommandBuffer commandBuffer,
                                                             VkBool32 primitiveRestartEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(primitiveRestartEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicPrimRestart] = true;

          renderstate.primRestartEnable = primitiveRestartEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetPrimitiveRestartEnable(Unwrap(commandBuffer), primitiveRestartEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer,
                                                   VkBool32 primitiveRestartEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetPrimitiveRestartEnable(Unwrap(commandBuffer), primitiveRestartEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetPrimitiveRestartEnable);
    Serialise_vkCmdSetPrimitiveRestartEnable(ser, commandBuffer, primitiveRestartEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetRasterizerDiscardEnable(SerialiserType &ser,
                                                              VkCommandBuffer commandBuffer,
                                                              VkBool32 rasterizerDiscardEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(rasterizerDiscardEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicRastDiscard] = true;

          renderstate.rastDiscardEnable = rasterizerDiscardEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetRasterizerDiscardEnable(Unwrap(commandBuffer), rasterizerDiscardEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer,
                                                    VkBool32 rasterizerDiscardEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetRasterizerDiscardEnable(Unwrap(commandBuffer), rasterizerDiscardEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetRasterizerDiscardEnable);
    Serialise_vkCmdSetRasterizerDiscardEnable(ser, commandBuffer, rasterizerDiscardEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetFragmentShadingRateKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer, const VkExtent2D *pFragmentSize,
    const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_OPT(pFragmentSize).Important();
  SERIALISE_ELEMENT_ARRAY(combinerOps, 4).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicShadingRateKHR] = true;

          renderstate.pipelineShadingRate = *pFragmentSize;
          renderstate.shadingRateCombiners[0] = combinerOps[0];
          renderstate.shadingRateCombiners[1] = combinerOps[1];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetFragmentShadingRateKHR(Unwrap(commandBuffer), pFragmentSize, combinerOps);
  }

  return true;
}

void WrappedVulkan::vkCmdSetFragmentShadingRateKHR(
    VkCommandBuffer commandBuffer, const VkExtent2D *pFragmentSize,
    const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetFragmentShadingRateKHR(Unwrap(commandBuffer), pFragmentSize, combinerOps));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetFragmentShadingRateKHR);
    Serialise_vkCmdSetFragmentShadingRateKHR(ser, commandBuffer, pFragmentSize, combinerOps);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetAttachmentFeedbackLoopEnableEXT(SerialiserType &ser,
                                                                      VkCommandBuffer commandBuffer,
                                                                      VkImageAspectFlags aspectMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(aspectMask).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicAttachmentFeedbackLoopEnableEXT] = true;

          renderstate.feedbackAspects = aspectMask;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetAttachmentFeedbackLoopEnableEXT(Unwrap(commandBuffer), aspectMask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetAttachmentFeedbackLoopEnableEXT(VkCommandBuffer commandBuffer,
                                                            VkImageAspectFlags aspectMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetAttachmentFeedbackLoopEnableEXT(Unwrap(commandBuffer), aspectMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetAttachmentFeedbackLoopEnableEXT);
    Serialise_vkCmdSetAttachmentFeedbackLoopEnableEXT(ser, commandBuffer, aspectMask);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetAlphaToCoverageEnableEXT(SerialiserType &ser,
                                                               VkCommandBuffer commandBuffer,
                                                               VkBool32 alphaToCoverageEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(alphaToCoverageEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicAlphaToCoverageEXT] = true;

          renderstate.alphaToCoverageEnable = alphaToCoverageEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetAlphaToCoverageEnableEXT(Unwrap(commandBuffer), alphaToCoverageEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer,
                                                     VkBool32 alphaToCoverageEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetAlphaToCoverageEnableEXT(Unwrap(commandBuffer), alphaToCoverageEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetAlphaToCoverageEnableEXT);
    Serialise_vkCmdSetAlphaToCoverageEnableEXT(ser, commandBuffer, alphaToCoverageEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetAlphaToOneEnableEXT(SerialiserType &ser,
                                                          VkCommandBuffer commandBuffer,
                                                          VkBool32 alphaToOneEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(alphaToOneEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicAlphaToOneEXT] = true;

          renderstate.alphaToOneEnable = alphaToOneEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetAlphaToOneEnableEXT(Unwrap(commandBuffer), alphaToOneEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer,
                                                VkBool32 alphaToOneEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetAlphaToOneEnableEXT(Unwrap(commandBuffer), alphaToOneEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetAlphaToOneEnableEXT);
    Serialise_vkCmdSetAlphaToOneEnableEXT(ser, commandBuffer, alphaToOneEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

void WrappedVulkan::vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer,
                                                  uint32_t firstAttachment, uint32_t attachmentCount,
                                                  const VkColorBlendAdvancedEXT *pColorBlendAdvanced)
{
  ILLEGAL_EDS3_CALL(vkCmdSetColorBlendAdvancedEXT);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetColorBlendEnableEXT(SerialiserType &ser,
                                                          VkCommandBuffer commandBuffer,
                                                          uint32_t firstAttachment,
                                                          uint32_t attachmentCount,
                                                          const VkBool32 *pColorBlendEnables)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstAttachment).Important();
  SERIALISE_ELEMENT(attachmentCount);
  SERIALISE_ELEMENT_ARRAY(pColorBlendEnables, attachmentCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicColorBlendEnableEXT] = true;

          if(renderstate.colorBlendEnable.size() < firstAttachment + attachmentCount)
            renderstate.colorBlendEnable.resize(firstAttachment + attachmentCount);

          for(uint32_t i = 0; i < attachmentCount; i++)
            renderstate.colorBlendEnable[firstAttachment + i] = pColorBlendEnables[i];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetColorBlendEnableEXT(Unwrap(commandBuffer), firstAttachment, attachmentCount,
                                      pColorBlendEnables);
  }

  return true;
}

void WrappedVulkan::vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer,
                                                uint32_t firstAttachment, uint32_t attachmentCount,
                                                const VkBool32 *pColorBlendEnables)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetColorBlendEnableEXT(Unwrap(commandBuffer), firstAttachment,
                                                      attachmentCount, pColorBlendEnables));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetColorBlendEnableEXT);
    Serialise_vkCmdSetColorBlendEnableEXT(ser, commandBuffer, firstAttachment, attachmentCount,
                                          pColorBlendEnables);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetColorBlendEquationEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstAttachment,
    uint32_t attachmentCount, const VkColorBlendEquationEXT *pColorBlendEquations)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstAttachment).Important();
  SERIALISE_ELEMENT(attachmentCount);
  SERIALISE_ELEMENT_ARRAY(pColorBlendEquations, attachmentCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicColorBlendEquationEXT] = true;

          if(renderstate.colorBlendEquation.size() < firstAttachment + attachmentCount)
            renderstate.colorBlendEquation.resize(firstAttachment + attachmentCount);

          for(uint32_t i = 0; i < attachmentCount; i++)
            renderstate.colorBlendEquation[firstAttachment + i] = pColorBlendEquations[i];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetColorBlendEquationEXT(Unwrap(commandBuffer), firstAttachment, attachmentCount,
                                        pColorBlendEquations);
  }

  return true;
}

void WrappedVulkan::vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer,
                                                  uint32_t firstAttachment, uint32_t attachmentCount,
                                                  const VkColorBlendEquationEXT *pColorBlendEquations)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetColorBlendEquationEXT(Unwrap(commandBuffer), firstAttachment,
                                                        attachmentCount, pColorBlendEquations));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetColorBlendEquationEXT);
    Serialise_vkCmdSetColorBlendEquationEXT(ser, commandBuffer, firstAttachment, attachmentCount,
                                            pColorBlendEquations);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetColorWriteMaskEXT(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        uint32_t firstAttachment,
                                                        uint32_t attachmentCount,
                                                        const VkColorComponentFlags *pColorWriteMasks)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstAttachment).Important();
  SERIALISE_ELEMENT(attachmentCount);
  SERIALISE_ELEMENT_ARRAY(pColorWriteMasks, attachmentCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicColorWriteMaskEXT] = true;

          if(renderstate.colorWriteMask.size() < firstAttachment + attachmentCount)
            renderstate.colorWriteMask.resize(firstAttachment + attachmentCount);

          for(uint32_t i = 0; i < attachmentCount; i++)
            renderstate.colorWriteMask[firstAttachment + i] = pColorWriteMasks[i];
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetColorWriteMaskEXT(Unwrap(commandBuffer), firstAttachment, attachmentCount,
                                    pColorWriteMasks);
  }

  return true;
}

void WrappedVulkan::vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer,
                                              uint32_t firstAttachment, uint32_t attachmentCount,
                                              const VkColorComponentFlags *pColorWriteMasks)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetColorWriteMaskEXT(Unwrap(commandBuffer), firstAttachment,
                                                    attachmentCount, pColorWriteMasks));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetColorWriteMaskEXT);
    Serialise_vkCmdSetColorWriteMaskEXT(ser, commandBuffer, firstAttachment, attachmentCount,
                                        pColorWriteMasks);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetConservativeRasterizationModeEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    VkConservativeRasterizationModeEXT conservativeRasterizationMode)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(conservativeRasterizationMode).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicConservativeRastModeEXT] = true;

          renderstate.conservativeRastMode = conservativeRasterizationMode;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetConservativeRasterizationModeEXT(Unwrap(commandBuffer),
                                                   conservativeRasterizationMode);
  }

  return true;
}

void WrappedVulkan::vkCmdSetConservativeRasterizationModeEXT(
    VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetConservativeRasterizationModeEXT(Unwrap(commandBuffer),
                                                                   conservativeRasterizationMode));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetConservativeRasterizationModeEXT);
    Serialise_vkCmdSetConservativeRasterizationModeEXT(ser, commandBuffer,
                                                       conservativeRasterizationMode);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

void WrappedVulkan::vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer,
                                                     VkCoverageModulationModeNV coverageModulationMode)
{
  ILLEGAL_EDS3_CALL(vkCmdSetCoverageModulationModeNV);
}

void WrappedVulkan::vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer,
                                                            VkBool32 coverageModulationTableEnable)
{
  ILLEGAL_EDS3_CALL(vkCmdSetCoverageModulationTableEnableNV);
}

void WrappedVulkan::vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer,
                                                      uint32_t coverageModulationTableCount,
                                                      const float *pCoverageModulationTable)
{
  ILLEGAL_EDS3_CALL(vkCmdSetCoverageModulationTableNV);
}

void WrappedVulkan::vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer,
                                                    VkCoverageReductionModeNV coverageReductionMode)
{
  ILLEGAL_EDS3_CALL(vkCmdSetCoverageReductionModeNV);
}

void WrappedVulkan::vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer,
                                                    VkBool32 coverageToColorEnable)
{
  ILLEGAL_EDS3_CALL(vkCmdSetCoverageToColorEnableNV);
}

void WrappedVulkan::vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer,
                                                      uint32_t coverageToColorLocation)
{
  ILLEGAL_EDS3_CALL(vkCmdSetCoverageToColorLocationNV);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthClampEnableEXT(SerialiserType &ser,
                                                          VkCommandBuffer commandBuffer,
                                                          VkBool32 depthClampEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthClampEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthClampEnableEXT] = true;

          renderstate.depthClampEnable = depthClampEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthClampEnableEXT(Unwrap(commandBuffer), depthClampEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer,
                                                VkBool32 depthClampEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthClampEnableEXT(Unwrap(commandBuffer), depthClampEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthClampEnableEXT);
    Serialise_vkCmdSetDepthClampEnableEXT(ser, commandBuffer, depthClampEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthClipEnableEXT(SerialiserType &ser,
                                                         VkCommandBuffer commandBuffer,
                                                         VkBool32 depthClipEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthClipEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthClipEnableEXT] = true;

          renderstate.depthClipEnable = depthClipEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthClipEnableEXT(Unwrap(commandBuffer), depthClipEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetDepthClipEnableEXT(Unwrap(commandBuffer), depthClipEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthClipEnableEXT);
    Serialise_vkCmdSetDepthClipEnableEXT(ser, commandBuffer, depthClipEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthClipNegativeOneToOneEXT(SerialiserType &ser,
                                                                   VkCommandBuffer commandBuffer,
                                                                   VkBool32 negativeOneToOne)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(negativeOneToOne).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicDepthClipNegativeOneEXT] = true;

          renderstate.negativeOneToOne = negativeOneToOne;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetDepthClipNegativeOneToOneEXT(Unwrap(commandBuffer), negativeOneToOne);
  }

  return true;
}

void WrappedVulkan::vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer,
                                                         VkBool32 negativeOneToOne)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetDepthClipNegativeOneToOneEXT(Unwrap(commandBuffer), negativeOneToOne));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDepthClipNegativeOneToOneEXT);
    Serialise_vkCmdSetDepthClipNegativeOneToOneEXT(ser, commandBuffer, negativeOneToOne);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetExtraPrimitiveOverestimationSizeEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(extraPrimitiveOverestimationSize).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicOverstimationSizeEXT] = true;

          renderstate.primOverestimationSize = extraPrimitiveOverestimationSize;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetExtraPrimitiveOverestimationSizeEXT(Unwrap(commandBuffer),
                                                      extraPrimitiveOverestimationSize);
  }

  return true;
}

void WrappedVulkan::vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer,
                                                                float extraPrimitiveOverestimationSize)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdSetExtraPrimitiveOverestimationSizeEXT(
                              Unwrap(commandBuffer), extraPrimitiveOverestimationSize));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetExtraPrimitiveOverestimationSizeEXT);
    Serialise_vkCmdSetExtraPrimitiveOverestimationSizeEXT(ser, commandBuffer,
                                                          extraPrimitiveOverestimationSize);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLineRasterizationModeEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    VkLineRasterizationModeKHR lineRasterizationMode)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(lineRasterizationMode).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicLineRastModeEXT] = true;

          renderstate.lineRasterMode = lineRasterizationMode;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetLineRasterizationModeEXT(Unwrap(commandBuffer), lineRasterizationMode);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer,
                                                     VkLineRasterizationModeKHR lineRasterizationMode)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetLineRasterizationModeEXT(Unwrap(commandBuffer), lineRasterizationMode));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLineRasterizationModeEXT);
    Serialise_vkCmdSetLineRasterizationModeEXT(ser, commandBuffer, lineRasterizationMode);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLineStippleEnableEXT(SerialiserType &ser,
                                                           VkCommandBuffer commandBuffer,
                                                           VkBool32 stippledLineEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(stippledLineEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicLineStippleEnableEXT] = true;

          renderstate.stippledLineEnable = stippledLineEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetLineStippleEnableEXT(Unwrap(commandBuffer), stippledLineEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer,
                                                 VkBool32 stippledLineEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetLineStippleEnableEXT(Unwrap(commandBuffer), stippledLineEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLineStippleEnableEXT);
    Serialise_vkCmdSetLineStippleEnableEXT(ser, commandBuffer, stippledLineEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLogicOpEnableEXT(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkBool32 logicOpEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(logicOpEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicLogicOpEnableEXT] = true;

          renderstate.logicOpEnable = logicOpEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetLogicOpEnableEXT(Unwrap(commandBuffer), logicOpEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetLogicOpEnableEXT(Unwrap(commandBuffer), logicOpEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLogicOpEnableEXT);
    Serialise_vkCmdSetLogicOpEnableEXT(ser, commandBuffer, logicOpEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetPolygonModeEXT(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     VkPolygonMode polygonMode)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(polygonMode).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicPolygonModeEXT] = true;

          renderstate.polygonMode = polygonMode;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetPolygonModeEXT(Unwrap(commandBuffer), polygonMode);
  }

  return true;
}

void WrappedVulkan::vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetPolygonModeEXT(Unwrap(commandBuffer), polygonMode));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetPolygonModeEXT);
    Serialise_vkCmdSetPolygonModeEXT(ser, commandBuffer, polygonMode);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetProvokingVertexModeEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(provokingVertexMode).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicProvokingVertexModeEXT] = true;

          renderstate.provokingVertexMode = provokingVertexMode;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetProvokingVertexModeEXT(Unwrap(commandBuffer), provokingVertexMode);
  }

  return true;
}

void WrappedVulkan::vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer,
                                                   VkProvokingVertexModeEXT provokingVertexMode)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetProvokingVertexModeEXT(Unwrap(commandBuffer), provokingVertexMode));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetProvokingVertexModeEXT);
    Serialise_vkCmdSetProvokingVertexModeEXT(ser, commandBuffer, provokingVertexMode);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetRasterizationSamplesEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(rasterizationSamples).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicRasterizationSamplesEXT] = true;

          renderstate.rastSamples = rasterizationSamples;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetRasterizationSamplesEXT(Unwrap(commandBuffer), rasterizationSamples);
  }

  return true;
}

void WrappedVulkan::vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer,
                                                    VkSampleCountFlagBits rasterizationSamples)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetRasterizationSamplesEXT(Unwrap(commandBuffer), rasterizationSamples));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetRasterizationSamplesEXT);
    Serialise_vkCmdSetRasterizationSamplesEXT(ser, commandBuffer, rasterizationSamples);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetRasterizationStreamEXT(SerialiserType &ser,
                                                             VkCommandBuffer commandBuffer,
                                                             uint32_t rasterizationStream)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(rasterizationStream).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicRasterizationStreamEXT] = true;

          renderstate.rasterStream = rasterizationStream;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetRasterizationStreamEXT(Unwrap(commandBuffer), rasterizationStream);
  }

  return true;
}

void WrappedVulkan::vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer,
                                                   uint32_t rasterizationStream)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetRasterizationStreamEXT(Unwrap(commandBuffer), rasterizationStream));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetRasterizationStreamEXT);
    Serialise_vkCmdSetRasterizationStreamEXT(ser, commandBuffer, rasterizationStream);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

void WrappedVulkan::vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer,
                                                               VkBool32 representativeFragmentTestEnable)
{
  ILLEGAL_EDS3_CALL(vkCmdSetRepresentativeFragmentTestEnableNV);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetSampleLocationsEnableEXT(SerialiserType &ser,
                                                               VkCommandBuffer commandBuffer,
                                                               VkBool32 sampleLocationsEnable)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(sampleLocationsEnable).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicSampleLocationsEnableEXT] = true;

          renderstate.sampleLocEnable = sampleLocationsEnable;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetSampleLocationsEnableEXT(Unwrap(commandBuffer), sampleLocationsEnable);
  }

  return true;
}

void WrappedVulkan::vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer,
                                                     VkBool32 sampleLocationsEnable)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetSampleLocationsEnableEXT(Unwrap(commandBuffer), sampleLocationsEnable));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetSampleLocationsEnableEXT);
    Serialise_vkCmdSetSampleLocationsEnableEXT(ser, commandBuffer, sampleLocationsEnable);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetSampleMaskEXT(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer,
                                                    VkSampleCountFlagBits samples,
                                                    const VkSampleMask *pSampleMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(samples).Important();
  SERIALISE_ELEMENT_ARRAY(pSampleMask, ((samples - 1) / 32) + 1).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicSampleMaskEXT] = true;

          renderstate.sampleMask.assign(pSampleMask, ((samples - 1) / 32) + 1);
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetSampleMaskEXT(Unwrap(commandBuffer), samples, pSampleMask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer,
                                          VkSampleCountFlagBits samples,
                                          const VkSampleMask *pSampleMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetSampleMaskEXT(Unwrap(commandBuffer), samples, pSampleMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetSampleMaskEXT);
    Serialise_vkCmdSetSampleMaskEXT(ser, commandBuffer, samples, pSampleMask);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

void WrappedVulkan::vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer,
                                                     VkBool32 shadingRateImageEnable)
{
  ILLEGAL_EDS3_CALL(vkCmdSetShadingRateImageEnableNV);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetTessellationDomainOriginEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(domainOrigin).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicTessDomainOriginEXT] = true;

          renderstate.domainOrigin = domainOrigin;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetTessellationDomainOriginEXT(Unwrap(commandBuffer), domainOrigin);
  }

  return true;
}

void WrappedVulkan::vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer,
                                                        VkTessellationDomainOrigin domainOrigin)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetTessellationDomainOriginEXT(Unwrap(commandBuffer), domainOrigin));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetTessellationDomainOriginEXT);
    Serialise_vkCmdSetTessellationDomainOriginEXT(ser, commandBuffer, domainOrigin);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

void WrappedVulkan::vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                                              uint32_t viewportCount,
                                              const VkViewportSwizzleNV *pViewportSwizzles)
{
  ILLEGAL_EDS3_CALL(vkCmdSetViewportSwizzleNV);
}

void WrappedVulkan::vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer,
                                                     VkBool32 viewportWScalingEnable)
{
  ILLEGAL_EDS3_CALL(vkCmdSetViewportWScalingEnableNV);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetRayTracingPipelineStackSizeKHR(SerialiserType &ser,
                                                                     VkCommandBuffer commandBuffer,
                                                                     uint32_t pipelineStackSize)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineStackSize).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicRayTracingStackSizeKHR] = true;

          renderstate.rtStackSize = pipelineStackSize;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetRayTracingPipelineStackSizeKHR(Unwrap(commandBuffer), pipelineStackSize);
  }

  return true;
}

void WrappedVulkan::vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer,
                                                           uint32_t pipelineStackSize)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetRayTracingPipelineStackSizeKHR(Unwrap(commandBuffer), pipelineStackSize));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetRayTracingPipelineStackSizeKHR);
    Serialise_vkCmdSetRayTracingPipelineStackSizeKHR(ser, commandBuffer, pipelineStackSize);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetViewport, VkCommandBuffer commandBuffer,
                                uint32_t firstViewport, uint32_t viewportCount,
                                const VkViewport *pViewports);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetScissor, VkCommandBuffer commandBuffer,
                                uint32_t firstScissor, uint32_t scissorCount,
                                const VkRect2D *pScissors);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLineWidth, VkCommandBuffer commandBuffer,
                                float lineWidth);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthBias, VkCommandBuffer commandBuffer,
                                float depthBiasConstantFactor, float depthBiasClamp,
                                float depthBiasSlopeFactor);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetBlendConstants, VkCommandBuffer commandBuffer,
                                const float blendConstants[4]);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthBounds, VkCommandBuffer commandBuffer,
                                float minDepthBounds, float maxDepthBounds);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetStencilCompareMask, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, uint32_t compareMask);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetStencilWriteMask, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, uint32_t writeMask);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetStencilReference, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, uint32_t reference);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetSampleLocationsEXT, VkCommandBuffer commandBuffer,
                                const VkSampleLocationsInfoEXT *pSampleLocationsInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDiscardRectangleEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstDiscardRectangle, uint32_t discardRectangleCount,
                                const VkRect2D *pDiscardRectangles);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLineStippleKHR, VkCommandBuffer commandBuffer,
                                uint32_t lineStippleFactor, uint16_t lineStipplePattern);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetCullMode, VkCommandBuffer commandBuffer,
                                VkCullModeFlags cullMode);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetFrontFace, VkCommandBuffer commandBuffer,
                                VkFrontFace frontFace);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetPrimitiveTopology, VkCommandBuffer commandBuffer,
                                VkPrimitiveTopology primitiveTopology);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetViewportWithCount, VkCommandBuffer commandBuffer,
                                uint32_t viewportCount, const VkViewport *pViewports);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetScissorWithCount, VkCommandBuffer commandBuffer,
                                uint32_t scissorCount, const VkRect2D *pScissors);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthTestEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthTestEnable);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthWriteEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthWriteEnable);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthCompareOp, VkCommandBuffer commandBuffer,
                                VkCompareOp depthCompareOp);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthBoundsTestEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthBoundsTestEnable);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetStencilTestEnable, VkCommandBuffer commandBuffer,
                                VkBool32 stencilTestEnable);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetStencilOp, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp,
                                VkStencilOp depthFailOp, VkCompareOp compareOp);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetColorWriteEnableEXT, VkCommandBuffer commandBuffer,
                                uint32_t attachmentCount, const VkBool32 *pColorWriteEnables);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthBiasEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthBiasEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLogicOpEXT, VkCommandBuffer commandBuffer,
                                VkLogicOp logicOp);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetPatchControlPointsEXT, VkCommandBuffer commandBuffer,
                                uint32_t patchControlPoints);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetPrimitiveRestartEnable, VkCommandBuffer commandBuffer,
                                VkBool32 primitiveRestartEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetRasterizerDiscardEnable,
                                VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetFragmentShadingRateKHR, VkCommandBuffer commandBuffer,
                                const VkExtent2D *pFragmentSize,
                                const VkFragmentShadingRateCombinerOpKHR combinerOps[2]);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetAttachmentFeedbackLoopEnableEXT,
                                VkCommandBuffer commandBuffer, VkImageAspectFlags aspectMask);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetAlphaToCoverageEnableEXT,
                                VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetAlphaToOneEnableEXT, VkCommandBuffer commandBuffer,
                                VkBool32 alphaToOneEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetColorBlendEnableEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstAttachment, uint32_t attachmentCount,
                                const VkBool32 *pColorBlendEnables);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetColorBlendEquationEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstAttachment, uint32_t attachmentCount,
                                const VkColorBlendEquationEXT *pColorBlendEquations);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetColorWriteMaskEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstAttachment, uint32_t attachmentCount,
                                const VkColorComponentFlags *pColorWriteMasks);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetConservativeRasterizationModeEXT,
                                VkCommandBuffer commandBuffer,
                                VkConservativeRasterizationModeEXT conservativeRasterizationMode);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthClampEnableEXT, VkCommandBuffer commandBuffer,
                                VkBool32 depthClampEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthClipEnableEXT, VkCommandBuffer commandBuffer,
                                VkBool32 depthClipEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDepthClipNegativeOneToOneEXT,
                                VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetExtraPrimitiveOverestimationSizeEXT,
                                VkCommandBuffer commandBuffer,
                                float extraPrimitiveOverestimationSize);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLineRasterizationModeEXT, VkCommandBuffer commandBuffer,
                                VkLineRasterizationModeKHR lineRasterizationMode);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLineStippleEnableEXT, VkCommandBuffer commandBuffer,
                                VkBool32 stippledLineEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLogicOpEnableEXT, VkCommandBuffer commandBuffer,
                                VkBool32 logicOpEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetPolygonModeEXT, VkCommandBuffer commandBuffer,
                                VkPolygonMode polygonMode);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetProvokingVertexModeEXT, VkCommandBuffer commandBuffer,
                                VkProvokingVertexModeEXT provokingVertexMode);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetRasterizationSamplesEXT, VkCommandBuffer commandBuffer,
                                VkSampleCountFlagBits rasterizationSamples);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetRasterizationStreamEXT, VkCommandBuffer commandBuffer,
                                uint32_t rasterizationStream);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetSampleLocationsEnableEXT,
                                VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetSampleMaskEXT, VkCommandBuffer commandBuffer,
                                VkSampleCountFlagBits samples, const VkSampleMask *pSampleMask);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetTessellationDomainOriginEXT,
                                VkCommandBuffer commandBuffer,
                                VkTessellationDomainOrigin domainOrigin);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetRayTracingPipelineStackSizeKHR,
                                VkCommandBuffer commandBuffer, uint32_t pipelineStackSize)
