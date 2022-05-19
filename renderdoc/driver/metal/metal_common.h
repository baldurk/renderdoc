/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "api/replay/rdcstr.h"
#include "common/common.h"
#include "common/timing.h"
#include "official/metal-cpp.h"
#include "serialise/serialiser.h"
#include "metal_resources.h"
#include "metal_types.h"

enum class MetalChunk : uint32_t
{
  MTLCreateSystemDefaultDevice = (uint32_t)SystemChunk::FirstDriverChunk,
  MTLDevice_newCommandQueue,
  MTLDevice_newCommandQueueWithMaxCommandBufferCount,
  MTLDevice_newHeapWithDescriptor,
  MTLDevice_newBufferWithLength,
  MTLDevice_newBufferWithBytes,
  MTLDevice_newBufferWithBytesNoCopy,
  MTLDevice_newDepthStencilStateWithDescriptor,
  MTLDevice_newTextureWithDescriptor,
  MTLDevice_newTextureWithDescriptor_iosurface,
  MTLDevice_newTextureWithDescriptor_nextDrawable,
  MTLDevice_newSharedTextureWithDescriptor,
  MTLDevice_newSharedTextureWithHandle,
  MTLDevice_newSamplerStateWithDescriptor,
  MTLDevice_newDefaultLibrary,
  MTLDevice_newDefaultLibraryWithBundle,
  MTLDevice_newLibraryWithFile,
  MTLDevice_newLibraryWithURL,
  MTLDevice_newLibraryWithData,
  MTLDevice_newLibraryWithSource,
  MTLDevice_newLibraryWithStitchedDescriptor,
  MTLDevice_newRenderPipelineStateWithDescriptor,
  MTLDevice_newRenderPipelineStateWithDescriptor_options,
  MTLDevice_newComputePipelineStateWithFunction,
  MTLDevice_newComputePipelineStateWithFunction_options,
  MTLDevice_newComputePipelineStateWithDescriptor,
  MTLDevice_newFence,
  MTLDevice_newRenderPipelineStateWithTileDescriptor,
  MTLDevice_newArgumentEncoderWithArguments,
  MTLDevice_supportsRasterizationRateMapWithLayerCount,
  MTLDevice_newRasterizationRateMapWithDescriptor,
  MTLDevice_newIndirectCommandBufferWithDescriptor,
  MTLDevice_newEvent,
  MTLDevice_newSharedEvent,
  MTLDevice_newSharedEventWithHandle,
  MTLDevice_newCounterSampleBufferWithDescriptor,
  MTLDevice_newDynamicLibrary,
  MTLDevice_newDynamicLibraryWithURL,
  MTLDevice_newBinaryArchiveWithDescriptor,
  MTLLibrary_newFunctionWithName,
  MTLLibrary_newFunctionWithName_constantValues,
  MTLLibrary_newFunctionWithDescriptor,
  MTLLibrary_newIntersectionFunctionWithDescriptor,
  MTLFunction_newArgumentEncoderWithBufferIndex,
  MTLCommandQueue_commandBuffer,
  MTLCommandQueue_commandBufferWithDescriptor,
  MTLCommandQueue_commandBufferWithUnretainedReferences,
  MTLCommandBuffer_enqueue,
  MTLCommandBuffer_commit,
  MTLCommandBuffer_addScheduledHandler,
  MTLCommandBuffer_presentDrawable,
  MTLCommandBuffer_presentDrawable_atTime,
  MTLCommandBuffer_presentDrawable_afterMinimumDuration,
  MTLCommandBuffer_waitUntilScheduled,
  MTLCommandBuffer_addCompletedHandler,
  MTLCommandBuffer_waitUntilCompleted,
  MTLCommandBuffer_blitCommandEncoder,
  MTLCommandBuffer_renderCommandEncoderWithDescriptor,
  MTLCommandBuffer_computeCommandEncoderWithDescriptor,
  MTLCommandBuffer_blitCommandEncoderWithDescriptor,
  MTLCommandBuffer_computeCommandEncoder,
  MTLCommandBuffer_computeCommandEncoderWithDispatchType,
  MTLCommandBuffer_encodeWaitForEvent,
  MTLCommandBuffer_encodeSignalEvent,
  MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor,
  MTLCommandBuffer_resourceStateCommandEncoder,
  MTLCommandBuffer_resourceStateCommandEncoderWithDescriptor,
  MTLCommandBuffer_accelerationStructureCommandEncoder,
  MTLCommandBuffer_pushDebugGroup,
  MTLCommandBuffer_popDebugGroup,
  MTLTexture_setPurgeableState,
  MTLTexture_makeAliasable,
  MTLTexture_getBytes,
  MTLTexture_getBytes_slice,
  MTLTexture_replaceRegion,
  MTLTexture_replaceRegion_slice,
  MTLTexture_newTextureViewWithPixelFormat,
  MTLTexture_newTextureViewWithPixelFormat_subset,
  MTLTexture_newTextureViewWithPixelFormat_subset_swizzle,
  MTLTexture_newSharedTextureHandle,
  MTLTexture_remoteStorageTexture,
  MTLTexture_newRemoteTextureViewForDevice,
  MTLRenderPipelineState_functionHandleWithFunction,
  MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor,
  MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor,
  MTLRenderPipelineState_newRenderPipelineStateWithAdditionalBinaryFunctions,
  Max
};

DECLARE_REFLECTION_ENUM(MetalChunk);

// must be at the start of any function that serialises
#define CACHE_THREAD_SERIALISER() WriteSerialiser &ser = m_Device->GetThreadSerialiser();

#define SERIALISE_TIME_CALL(...)                                                                \
  {                                                                                             \
    WriteSerialiser &ser = m_Device->GetThreadSerialiser();                                     \
    ser.ChunkMetadata().timestampMicro = Timing::GetTick();                                     \
    __VA_ARGS__;                                                                                \
    ser.ChunkMetadata().durationMicro = Timing::GetTick() - ser.ChunkMetadata().timestampMicro; \
  }

#define DECLARE_FUNCTION_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                            \
  template <typename SerialiserType>                \
  bool CONCAT(Serialise_, func(SerialiserType &ser, ##__VA_ARGS__));

#define INSTANTIATE_FUNCTION_SERIALISED(CLASS, ret, func, ...)                       \
  template bool CLASS::CONCAT(Serialise_, func(ReadSerialiser &ser, ##__VA_ARGS__)); \
  template bool CLASS::CONCAT(Serialise_, func(WriteSerialiser &ser, ##__VA_ARGS__));

#define DECLARE_FUNCTION_WITH_RETURN_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                                        \
  template <typename SerialiserType>                            \
  bool CONCAT(Serialise_, func(SerialiserType &ser, ret, ##__VA_ARGS__));

#define INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(CLASS, ret, func, ...)                \
  template bool CLASS::CONCAT(Serialise_, func(ReadSerialiser &ser, ret, ##__VA_ARGS__)); \
  template bool CLASS::CONCAT(Serialise_, func(WriteSerialiser &ser, ret, ##__VA_ARGS__));

// A handy macro to say "is the serialiser reading and we're doing replay-mode stuff?"
// The reason we check both is that checking the first allows the compiler to eliminate the other
// path at compile-time, and the second because we might be just struct-serialising in which case we
// should be doing no work to restore states.
// Writing is unambiguously during capture mode, so we don't have to check both in that case.
#define IsReplayingAndReading() (ser.IsReading() && IsReplayMode(m_Device->GetState()))

#ifdef __OBJC__
#define METAL_NOT_HOOKED()                                                          \
  do                                                                                \
  {                                                                                 \
    RDCERR("Metal %s %s not hooked", object_getClassName(self), sel_getName(_cmd)); \
  } while((void)0, 0)
#endif

// similar to RDCUNIMPLEMENTED but without the debugbreak
#define METAL_NOT_IMPLEMENTED(...)                                            \
  do                                                                          \
  {                                                                           \
    RDCWARN("Metal '%s' not implemented -" __VA_ARGS__, __PRETTY_FUNCTION__); \
  } while((void)0, 0)

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define METAL_NOT_IMPLEMENTED_ONCE(...)                                          \
  do                                                                             \
  {                                                                              \
    static bool msgprinted = false;                                              \
    if(!msgprinted)                                                              \
      RDCWARN("Metal '%s' not implemented - " __VA_ARGS__, __PRETTY_FUNCTION__); \
    msgprinted = true;                                                           \
  } while((void)0, 0)
