/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#pragma once

#include "metal_common.h"
#include "metal_device.h"
#include "metal_resources.h"

class WrappedMTLBlitCommandEncoder : public WrappedMTLObject
{
public:
  WrappedMTLBlitCommandEncoder(MTL::BlitCommandEncoder *realMTLBlitCommandEncoder, ResourceId objId,
                               WrappedMTLDevice *wrappedMTLDevice);

  void SetCommandBuffer(WrappedMTLCommandBuffer *commandBuffer) { m_CommandBuffer = commandBuffer; }
  DECLARE_FUNCTION_SERIALISED(void, setLabel, NS::String *value);
  DECLARE_FUNCTION_SERIALISED(void, endEncoding);
  DECLARE_FUNCTION_SERIALISED(void, insertDebugSignpost, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, pushDebugGroup, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, popDebugGroup);
  DECLARE_FUNCTION_SERIALISED(void, synchronizeResource, WrappedMTLResource *resource);
  DECLARE_FUNCTION_SERIALISED(void, synchronizeTexture, WrappedMTLTexture *texture,
                              NS::UInteger slice, NS::UInteger level);
  DECLARE_FUNCTION_SERIALISED(void, copyFromBuffer, WrappedMTLBuffer *sourceBuffer,
                              NS::UInteger sourceOffset, WrappedMTLBuffer *destinationBuffer,
                              NS::UInteger destinationOffset, NS::UInteger size);
  DECLARE_FUNCTION_SERIALISED(void, copyFromBuffer, WrappedMTLBuffer *sourceBuffer,
                              NS::UInteger sourceOffset, NS::UInteger sourceBytesPerRow,
                              NS::UInteger sourceBytesPerImage, MTL::Size &sourceSize,
                              WrappedMTLTexture *destinationTexture, NS::UInteger destinationSlice,
                              NS::UInteger destinationLevel, MTL::Origin &destinationOrigin,
                              MTL::BlitOption options);
  DECLARE_FUNCTION_SERIALISED(void, copyFromTexture, WrappedMTLTexture *sourceTexture,
                              NS::UInteger sourceSlice, NS::UInteger sourceLevel,
                              MTL::Origin &sourceOrigin, MTL::Size &sourceSize,
                              WrappedMTLTexture *destinationTexture, NS::UInteger destinationSlice,
                              NS::UInteger destinationLevel, MTL::Origin &destinationOrigin);
  DECLARE_FUNCTION_SERIALISED(void, copyFromTexture, WrappedMTLTexture *sourceTexture,
                              NS::UInteger sourceSlice, NS::UInteger sourceLevel,
                              MTL::Origin &sourceOrigin, MTL::Size &sourceSize,
                              WrappedMTLBuffer *destinationBuffer, NS::UInteger destinationOffset,
                              NS::UInteger destinationBytesPerRow,
                              NS::UInteger destinationBytesPerImage, MTL::BlitOption options);
  DECLARE_FUNCTION_SERIALISED(void, copyFromTexture, WrappedMTLTexture *sourceTexture,
                              WrappedMTLTexture *destinationTexture);
  DECLARE_FUNCTION_SERIALISED(void, copyFromTexture, WrappedMTLTexture *sourceTexture,
                              NS::UInteger sourceSlice, NS::UInteger sourceLevel,
                              WrappedMTLTexture *destinationTexture, NS::UInteger destinationSlice,
                              NS::UInteger destinationLevel, NS::UInteger sliceCount,
                              NS::UInteger levelCount);
  DECLARE_FUNCTION_SERIALISED(void, generateMipmapsForTexture, WrappedMTLTexture *texture);
  DECLARE_FUNCTION_SERIALISED(void, fillBuffer, WrappedMTLBuffer *buffer, NS::Range &range,
                              uint8_t value);
  DECLARE_FUNCTION_SERIALISED(void, updateFence, WrappedMTLFence *fence);
  DECLARE_FUNCTION_SERIALISED(void, waitForFence, WrappedMTLFence *fence);
  DECLARE_FUNCTION_SERIALISED(void, getTextureAccessCounters, WrappedMTLTexture *texture,
                              MTL::Region &region, NS::UInteger mipLevel, NS::UInteger slice,
                              bool resetCounters, WrappedMTLBuffer *countersBuffer,
                              NS::UInteger countersBufferOffset);
  DECLARE_FUNCTION_SERIALISED(void, resetTextureAccessCounters, WrappedMTLTexture *texture,
                              MTL::Region &region, NS::UInteger mipLevel, NS::UInteger slice);
  DECLARE_FUNCTION_SERIALISED(void, optimizeContentsForGPUAccess, WrappedMTLTexture *texture);
  DECLARE_FUNCTION_SERIALISED(void, optimizeContentsForGPUAccess, WrappedMTLTexture *texture,
                              NS::UInteger slice, NS::UInteger level);
  DECLARE_FUNCTION_SERIALISED(void, optimizeContentsForCPUAccess, WrappedMTLTexture *texture);
  DECLARE_FUNCTION_SERIALISED(void, optimizeContentsForCPUAccess, WrappedMTLTexture *texture,
                              NS::UInteger slice, NS::UInteger level);
  DECLARE_FUNCTION_SERIALISED(void, resetCommandsInBuffer, WrappedMTLIndirectCommandBuffer *buffer,
                              NS::Range &range);
  DECLARE_FUNCTION_SERIALISED(void, copyIndirectCommandBuffer,
                              WrappedMTLIndirectCommandBuffer *source, NS::Range &sourceRange,
                              WrappedMTLIndirectCommandBuffer *destination,
                              NS::UInteger destinationIndex);
  DECLARE_FUNCTION_SERIALISED(void, optimizeIndirectCommandBuffer,
                              WrappedMTLIndirectCommandBuffer *indirectCommandBuffer,
                              NS::Range &range);
  DECLARE_FUNCTION_SERIALISED(void, sampleCountersInBuffer,
                              WrappedMTLCounterSampleBuffer *sampleBuffer, NS::UInteger sampleIndex,
                              bool barrier);
  DECLARE_FUNCTION_SERIALISED(void, resolveCounters, WrappedMTLCounterSampleBuffer *sampleBuffer,
                              NS::Range &range, WrappedMTLBuffer *destinationBuffer,
                              NS::UInteger destinationOffset);

  enum
  {
    TypeEnum = eResBlitCommandEncoder
  };

private:
  WrappedMTLCommandBuffer *m_CommandBuffer;
};
