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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetViewport(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               uint32_t firstViewport, uint32_t viewportCount,
                                               const VkViewport *pViewports)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstViewport);
  SERIALISE_ELEMENT(viewportCount);
  SERIALISE_ELEMENT_ARRAY(pViewports, viewportCount);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(m_RenderState.views.size() < firstViewport + viewportCount)
            m_RenderState.views.resize(firstViewport + viewportCount);

          for(uint32_t i = 0; i < viewportCount; i++)
            m_RenderState.views[firstViewport + i] = pViewports[i];
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetScissor(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              uint32_t firstScissor, uint32_t scissorCount,
                                              const VkRect2D *pScissors)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstScissor);
  SERIALISE_ELEMENT(scissorCount);
  SERIALISE_ELEMENT_ARRAY(pScissors, scissorCount);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(m_RenderState.scissors.size() < firstScissor + scissorCount)
            m_RenderState.scissors.resize(firstScissor + scissorCount);

          for(uint32_t i = 0; i < scissorCount; i++)
            m_RenderState.scissors[firstScissor + i] = pScissors[i];
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetLineWidth(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                float lineWidth)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(lineWidth);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
          m_RenderState.lineWidth = lineWidth;
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthBias(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                float depthBias, float depthBiasClamp,
                                                float slopeScaledDepthBias)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(depthBias);
  SERIALISE_ELEMENT(depthBiasClamp);
  SERIALISE_ELEMENT(slopeScaledDepthBias);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.bias.depth = depthBias;
          m_RenderState.bias.biasclamp = depthBiasClamp;
          m_RenderState.bias.slope = slopeScaledDepthBias;
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetBlendConstants(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     const float *blendConst)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_ARRAY(blendConst, 4);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
          memcpy(m_RenderState.blendConst, blendConst, sizeof(m_RenderState.blendConst));
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDepthBounds(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  float minDepthBounds, float maxDepthBounds)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(minDepthBounds);
  SERIALISE_ELEMENT(maxDepthBounds);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.mindepth = minDepthBounds;
          m_RenderState.maxdepth = maxDepthBounds;
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilCompareMask(SerialiserType &ser,
                                                         VkCommandBuffer commandBuffer,
                                                         VkStencilFaceFlags faceMask,
                                                         uint32_t compareMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).TypedAs("VkStencilFaceFlags"_lit);
  SERIALISE_ELEMENT(compareMask);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
            m_RenderState.front.compare = compareMask;
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
            m_RenderState.back.compare = compareMask;
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilWriteMask(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkStencilFaceFlags faceMask,
                                                       uint32_t writeMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).TypedAs("VkStencilFaceFlags"_lit);
  SERIALISE_ELEMENT(writeMask);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
            m_RenderState.front.write = writeMask;
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
            m_RenderState.back.write = writeMask;
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetStencilReference(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkStencilFaceFlags faceMask,
                                                       uint32_t reference)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkStencilFaceFlagBits, faceMask).TypedAs("VkStencilFaceFlags"_lit);
  SERIALISE_ELEMENT(reference);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(faceMask & VK_STENCIL_FACE_FRONT_BIT)
            m_RenderState.front.ref = reference;
          if(faceMask & VK_STENCIL_FACE_BACK_BIT)
            m_RenderState.back.ref = reference;
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

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetSampleLocationsEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkSampleLocationsInfoEXT *pSampleLocationsInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(sampleInfo, *pSampleLocationsInfo).Named("pSampleLocationsInfo"_lit);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.sampleLocations.locations.clear();
          m_RenderState.sampleLocations.locations.insert(
              m_RenderState.sampleLocations.locations.begin(), sampleInfo.pSampleLocations,
              sampleInfo.pSampleLocations + sampleInfo.sampleLocationsCount);
          m_RenderState.sampleLocations.gridSize = sampleInfo.sampleLocationGridSize;
          m_RenderState.sampleLocations.sampleCount = sampleInfo.sampleLocationsPerPixel;
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

    record->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(firstDiscardRectangle);
  SERIALISE_ELEMENT(discardRectangleCount);
  SERIALISE_ELEMENT_ARRAY(pDiscardRectangles, discardRectangleCount);

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

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(m_RenderState.discardRectangles.size() < firstDiscardRectangle + discardRectangleCount)
            m_RenderState.discardRectangles.resize(firstDiscardRectangle + discardRectangleCount);

          for(uint32_t i = 0; i < discardRectangleCount; i++)
            m_RenderState.discardRectangles[firstDiscardRectangle + i] = pDiscardRectangles[i];
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

    record->AddChunk(scope.Get());
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