/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
          GetCmdRenderState().lineWidth = lineWidth;
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
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask)
      .TypedAs("VkStencilFaceFlags"_lit)
      .Important();
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
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask)
      .TypedAs("VkStencilFaceFlags"_lit)
      .Important();
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
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask)
      .TypedAs("VkStencilFaceFlags"_lit)
      .Important();
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
  SERIALISE_ELEMENT_LOCAL(sampleInfo, *pSampleLocationsInfo)
      .Named("pSampleLocationsInfo"_lit)
      .Important();

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
bool WrappedVulkan::Serialise_vkCmdSetLineStippleEXT(SerialiserType &ser,
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
          renderstate.stippleFactor = lineStippleFactor;
          renderstate.stipplePattern = lineStipplePattern;
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdSetLineStippleEXT(Unwrap(commandBuffer), lineStippleFactor, lineStipplePattern);
  }

  return true;
}

void WrappedVulkan::vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer,
                                           uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetLineStippleEXT(Unwrap(commandBuffer), lineStippleFactor, lineStipplePattern));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetLineStippleEXT);
    Serialise_vkCmdSetLineStippleEXT(ser, commandBuffer, lineStippleFactor, lineStipplePattern);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
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

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetLineStippleEXT, VkCommandBuffer commandBuffer,
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
