/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 Baldur Karlsson
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

#include "metal_common.h"

#define MTL_STRINGISE_ENUM(a) STRINGISE_ENUM_CLASS_NAMED(a, "MTL" #a)
#define MTL_STRINGISE_BITFIELD_BIT(a) STRINGISE_BITFIELD_CLASS_BIT_NAMED(a, "MTL" #a)
#define MTL_STRINGISE_BITFIELD_VALUE(a) STRINGISE_BITFIELD_CLASS_VALUE_NAMED(a, "MTL" #a)

template <>
rdcstr DoStringise(const MetalChunk &el)
{
  RDCCOMPILE_ASSERT((uint32_t)MetalChunk::Max == 1229, "Chunks changed without updating names");

  BEGIN_ENUM_STRINGISE(MetalChunk)
  {
    STRINGISE_ENUM_CLASS(MTLCreateSystemDefaultDevice);
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newCommandQueue, "MTLDevice::newCommandQueue");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newCommandQueueWithMaxCommandBufferCount,
                               "MTLDevice::newCommandQueueWithMaxCommandBufferCount");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newHeapWithDescriptor, "MTLDevice::newHeapWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newBufferWithLength, "MTLDevice::newBufferWithLength");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newBufferWithBytes, "MTLDevice::newBufferWithBytes");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newBufferWithBytesNoCopy,
                               "MTLDevice::newBufferWithBytesNoCopy");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newDepthStencilStateWithDescriptor,
                               "MTLDevice::newDepthStencilStateWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newTextureWithDescriptor,
                               "MTLDevice::newTextureWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newTextureWithDescriptor_iosurface,
                               "MTLDevice::newTextureWithDescriptor(iosurface, plane)");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newTextureWithDescriptor_nextDrawable,
                               "[CAMetalLayer nextDrawable]");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newSharedTextureWithDescriptor,
                               "MTLDevice::newSharedTextureWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newSharedTextureWithHandle,
                               "MTLDevice::newSharedTextureWithHandle");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newSamplerStateWithDescriptor,
                               "MTLDevice::newSamplerStateWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newDefaultLibrary, "MTLDevice::newDefaultLibrary");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newDefaultLibraryWithBundle,
                               "MTLDevice::newDefaultLibraryWithBundle");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newLibraryWithFile, "MTLDevice::newLibraryWithFile");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newLibraryWithURL, "MTLDevice::newLibraryWithURL");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newLibraryWithData, "MTLDevice::newLibraryWithData");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newLibraryWithSource, "MTLDevice::newLibraryWithSource");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newLibraryWithStitchedDescriptor,
                               "MTLDevice::newLibraryWithStitchedDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newRenderPipelineStateWithDescriptor,
                               "MTLDevice::newRenderPipelineStateWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newRenderPipelineStateWithDescriptor_options,
                               "MTLDevice::newRenderPipelineStateWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newComputePipelineStateWithFunction,
                               "MTLDevice::newComputePipelineStateWithFunction");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newComputePipelineStateWithFunction_options,
                               "MTLDevice::newComputePipelineStateWithFunction");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newComputePipelineStateWithDescriptor,
                               "MTLDevice::newComputePipelineStateWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newFence, "MTLDevice::newFence");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newRenderPipelineStateWithTileDescriptor,
                               "MTLDevice::newRenderPipelineStateWithTileDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newArgumentEncoderWithArguments,
                               "MTLDevice::newArgumentEncoderWithArguments");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_supportsRasterizationRateMapWithLayerCount,
                               "MTLDevice::supportsRasterizationRateMapWithLayerCount");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newRasterizationRateMapWithDescriptor,
                               "MTLDevice::newRasterizationRateMapWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newIndirectCommandBufferWithDescriptor,
                               "MTLDevice::newIndirectCommandBufferWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newEvent, "MTLDevice::newEvent");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newSharedEvent, "MTLDevice::newSharedEvent");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newSharedEventWithHandle,
                               "MTLDevice::newSharedEventWithHandle");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newCounterSampleBufferWithDescriptor,
                               "MTLDevice::newCounterSampleBufferWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newDynamicLibrary, "MTLDevice::newDynamicLibrary");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newDynamicLibraryWithURL,
                               "MTLDevice::newDynamicLibraryWithURL");
    STRINGISE_ENUM_CLASS_NAMED(MTLDevice_newBinaryArchiveWithDescriptor,
                               "MTLDevice::newBinaryArchiveWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLLibrary_newFunctionWithName, "MTLLibrary::newFunctionWithName");
    STRINGISE_ENUM_CLASS_NAMED(MTLLibrary_newFunctionWithName_constantValues,
                               "MTLLibrary::newFunctionWithName");
    STRINGISE_ENUM_CLASS_NAMED(MTLLibrary_newFunctionWithDescriptor,
                               "MTLLibrary::newFunctionWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLLibrary_newIntersectionFunctionWithDescriptor,
                               "MTLLibrary::newIntersectionFunctionWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLFunction_newArgumentEncoderWithBufferIndex,
                               "MTLFunction::newArgumentEncoderWithBufferIndex");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandQueue_commandBuffer, "MTLCommandQueue::commandBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandQueue_commandBufferWithDescriptor,
                               "MTLCommandQueue::commandBufferWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandQueue_commandBufferWithUnretainedReferences,
                               "MTLCommandQueue::commandBufferWithUnretainedReferences");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_enqueue, "MTLCommandBuffer::enqueue");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_commit, "MTLCommandBuffer::commit");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_addScheduledHandler,
                               "MTLCommandBuffer::addScheduledHandler");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_presentDrawable,
                               "MTLCommandBuffer::presentDrawable");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_presentDrawable_atTime,
                               "MTLCommandBuffer::presentDrawable");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_presentDrawable_afterMinimumDuration,
                               "MTLCommandBuffer::presentDrawable");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_waitUntilScheduled,
                               "MTLCommandBuffer::waitUntilScheduled");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_addCompletedHandler,
                               "MTLCommandBuffer::addCompletedHandler");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_waitUntilCompleted,
                               "MTLCommandBuffer::waitUntilCompleted");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_blitCommandEncoder,
                               "MTLCommandBuffer::blitCommandEncoder");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_renderCommandEncoderWithDescriptor,
                               "MTLCommandBuffer::renderCommandEncoderWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_computeCommandEncoderWithDescriptor,
                               "MTLCommandBuffer::computeCommandEncoderWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_blitCommandEncoderWithDescriptor,
                               "MTLCommandBuffer::blitCommandEncoderWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_computeCommandEncoder,
                               "MTLCommandBuffer::computeCommandEncoder");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_computeCommandEncoderWithDispatchType,
                               "MTLCommandBuffer::computeCommandEncoderWithDispatchType");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_encodeWaitForEvent,
                               "MTLCommandBuffer::encodeWaitForEvent");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_encodeSignalEvent,
                               "MTLCommandBuffer::encodeSignalEvent");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor,
                               "MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_resourceStateCommandEncoder,
                               "MTLCommandBuffer::resourceStateCommandEncoder");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_resourceStateCommandEncoderWithDescriptor,
                               "MTLCommandBuffer::resourceStateCommandEncoderWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_accelerationStructureCommandEncoder,
                               "MTLCommandBuffer::accelerationStructureCommandEncoder");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_pushDebugGroup, "MTLCommandBuffer::pushDebugGroup");
    STRINGISE_ENUM_CLASS_NAMED(MTLCommandBuffer_popDebugGroup, "MTLCommandBuffer::popDebugGroup");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_setPurgeableState, "MTLTexture::setPurgeableState");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_makeAliasable, "MTLTexture::makeAliasable");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_getBytes, "MTLTexture::getBytes");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_getBytes_slice, "MTLTexture::getBytes");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_replaceRegion, "MTLTexture::replaceRegion");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_replaceRegion_slice, "MTLTexture::replaceRegion");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_newTextureViewWithPixelFormat,
                               "MTLTexture::newTextureViewWithPixelFormat");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_newTextureViewWithPixelFormat_subset,
                               "MTLTexture::newTextureViewWithPixelFormat");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_newTextureViewWithPixelFormat_subset_swizzle,
                               "MTLTexture::newTextureViewWithPixelFormat");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_newSharedTextureHandle,
                               "MTLTexture::newSharedTextureHandle");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_remoteStorageTexture, "MTLTexture::remoteStorageTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLTexture_newRemoteTextureViewForDevice,
                               "MTLTexture::newRemoteTextureViewForDevice");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderPipelineState_functionHandleWithFunction,
                               "MTLRenderPipelineState::functionHandleWithFunction");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor,
                               "MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(
        MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor,
        "MTLRenderPipelineState::newIntersectionFunctionTableWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(
        MTLRenderPipelineState_newRenderPipelineStateWithAdditionalBinaryFunctions,
        "MTLRenderPipelineState::newRenderPipelineStateWithAdditionalBinaryFunctions");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_endEncoding,
                               "MTLRenderCommandEncoder::endEncoding");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_insertDebugSignpost,
                               "MTLRenderCommandEncoder::insertDebugSignpost");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_pushDebugGroup,
                               "MTLRenderCommandEncoder::pushDebugGroup");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_popDebugGroup,
                               "MTLRenderCommandEncoder::popDebugGroup");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setRenderPipelineState,
                               "MTLRenderCommandEncoder::setRenderPipelineState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexBytes,
                               "MTLRenderCommandEncoder::setVertexBytes");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexBuffer,
                               "MTLRenderCommandEncoder::setVertexBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexBufferOffset,
                               "MTLRenderCommandEncoder::setVertexBufferOffset");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexBuffers,
                               "MTLRenderCommandEncoder::setVertexBuffers");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexTexture,
                               "MTLRenderCommandEncoder::setVertexTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexTextures,
                               "MTLRenderCommandEncoder::setVertexTextures");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexSamplerState,
                               "MTLRenderCommandEncoder::setVertexSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexSamplerState_lodclamp,
                               "MTLRenderCommandEncoder::setVertexSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexSamplerStates,
                               "MTLRenderCommandEncoder::setVertexSamplerStates");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexSamplerStates_lodclamp,
                               "MTLRenderCommandEncoder::setVertexSamplerStates");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexVisibleFunctionTable,
                               "MTLRenderCommandEncoder::setVertexVisibleFunctionTable");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexVisibleFunctionTables,
                               "MTLRenderCommandEncoder::setVertexVisibleFunctionTables");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexIntersectionFunctionTable,
                               "MTLRenderCommandEncoder::setVertexIntersectionFunctionTable");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexIntersectionFunctionTables,
                               "MTLRenderCommandEncoder::setVertexIntersectionFunctionTables");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexAccelerationStructure,
                               "MTLRenderCommandEncoder::setVertexAccelerationStructure");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setViewport,
                               "MTLRenderCommandEncoder::setViewport");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setViewports,
                               "MTLRenderCommandEncoder::setViewports");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFrontFacingWinding,
                               "MTLRenderCommandEncoder::setFrontFacingWinding");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVertexAmplificationCount,
                               "MTLRenderCommandEncoder::setVertexAmplificationCount");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setCullMode,
                               "MTLRenderCommandEncoder::setCullMode");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setDepthClipMode,
                               "MTLRenderCommandEncoder::setDepthClipMode");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setDepthBias,
                               "MTLRenderCommandEncoder::setDepthBias");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setScissorRect,
                               "MTLRenderCommandEncoder::setScissorRect");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setScissorRects,
                               "MTLRenderCommandEncoder::setScissorRects");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTriangleFillMode,
                               "MTLRenderCommandEncoder::setTriangleFillMode");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentBytes,
                               "MTLRenderCommandEncoder::setFragmentBytes");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentBuffer,
                               "MTLRenderCommandEncoder::setFragmentBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentBufferOffset,
                               "MTLRenderCommandEncoder::setFragmentBufferOffset");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentBuffers,
                               "MTLRenderCommandEncoder::setFragmentBuffers");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentTexture,
                               "MTLRenderCommandEncoder::setFragmentTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentTextures,
                               "MTLRenderCommandEncoder::setFragmentTextures");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentSamplerState,
                               "MTLRenderCommandEncoder::setFragmentSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentSamplerState_lodclamp,
                               "MTLRenderCommandEncoder::setFragmentSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentSamplerStates,
                               "MTLRenderCommandEncoder::setFragmentSamplerStates");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentSamplerStates_lodclamp,
                               "MTLRenderCommandEncoder::setFragmentSamplerStates");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentVisibleFunctionTable,
                               "MTLRenderCommandEncoder::setFragmentVisibleFunctionTable");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentVisibleFunctionTables,
                               "MTLRenderCommandEncoder::setFragmentVisibleFunctionTables");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentIntersectionFunctionTable,
                               "MTLRenderCommandEncoder::setFragmentIntersectionFunctionTable");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentIntersectionFunctionTables,
                               "MTLRenderCommandEncoder::setFragmentIntersectionFunctionTables");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setFragmentAccelerationStructure,
                               "MTLRenderCommandEncoder::setFragmentAccelerationStructure");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setBlendColor,
                               "MTLRenderCommandEncoder::setBlendColor");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setDepthStencilState,
                               "MTLRenderCommandEncoder::setDepthStencilState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setStencilReferenceValue,
                               "MTLRenderCommandEncoder::setStencilReferenceValue");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setStencilFrontReferenceValue,
                               "MTLRenderCommandEncoder::setStencilFrontReferenceValue");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setVisibilityResultMode,
                               "MTLRenderCommandEncoder::setVisibilityResultMode");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setColorStoreAction,
                               "MTLRenderCommandEncoder::setColorStoreAction");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setDepthStoreAction,
                               "MTLRenderCommandEncoder::setDepthStoreAction");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setStencilStoreAction,
                               "MTLRenderCommandEncoder::setStencilStoreAction");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setColorStoreActionOptions,
                               "MTLRenderCommandEncoder::setColorStoreActionOptions");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setDepthStoreActionOptions,
                               "MTLRenderCommandEncoder::setDepthStoreActionOptions");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setStencilStoreActionOptions,
                               "MTLRenderCommandEncoder::setStencilStoreActionOptions");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawPrimitives,
                               "MTLRenderCommandEncoder::drawPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawPrimitives_instanced,
                               "MTLRenderCommandEncoder::drawPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawPrimitives_instanced_base,
                               "MTLRenderCommandEncoder::drawPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawPrimitives_indirect,
                               "MTLRenderCommandEncoder::drawPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawIndexedPrimitives,
                               "MTLRenderCommandEncoder::drawIndexedPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawIndexedPrimitives_instanced,
                               "MTLRenderCommandEncoder::drawIndexedPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base,
                               "MTLRenderCommandEncoder::drawIndexedPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawIndexedPrimitives_indirect,
                               "MTLRenderCommandEncoder::drawIndexedPrimitives");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_textureBarrier,
                               "MTLRenderCommandEncoder::textureBarrier");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_updateFence,
                               "MTLRenderCommandEncoder::updateFence");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_waitForFence,
                               "MTLRenderCommandEncoder::waitForFence");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTessellationFactorBuffer,
                               "MTLRenderCommandEncoder::setTessellationFactorBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTessellationFactorScale,
                               "MTLRenderCommandEncoder::setTessellationFactorScale");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawPatches,
                               "MTLRenderCommandEncoder::drawPatches");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawPatches_indirect,
                               "MTLRenderCommandEncoder::drawPatches");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawIndexedPatches,
                               "MTLRenderCommandEncoder::drawIndexedPatches");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_drawIndexedPatches_indirect,
                               "MTLRenderCommandEncoder::drawIndexedPatches");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileBytes,
                               "MTLRenderCommandEncoder::setTileBytes");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileBuffer,
                               "MTLRenderCommandEncoder::setTileBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileBufferOffset,
                               "MTLRenderCommandEncoder::setTileBufferOffset");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileBuffers,
                               "MTLRenderCommandEncoder::setTileBuffers");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileTexture,
                               "MTLRenderCommandEncoder::setTileTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileTextures,
                               "MTLRenderCommandEncoder::setTileTextures");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileSamplerState,
                               "MTLRenderCommandEncoder::setTileSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileSamplerState_lodclamp,
                               "MTLRenderCommandEncoder::setTileSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileSamplerStates,
                               "MTLRenderCommandEncoder::setTileSamplerStates");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileSamplerStates_lodclamp,
                               "MTLRenderCommandEncoder::setTileSamplerStates");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileVisibleFunctionTable,
                               "MTLRenderCommandEncoder::setTileVisibleFunctionTable");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileVisibleFunctionTables,
                               "MTLRenderCommandEncoder::setTileVisibleFunctionTables");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileIntersectionFunctionTable,
                               "MTLRenderCommandEncoder::setTileIntersectionFunctionTable");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileIntersectionFunctionTables,
                               "MTLRenderCommandEncoder::setTileIntersectionFunctionTables");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setTileAccelerationStructure,
                               "MTLRenderCommandEncoder::setTileAccelerationStructure");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_dispatchThreadsPerTile,
                               "MTLRenderCommandEncoder::dispatchThreadsPerTile");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_setThreadgroupMemoryLength,
                               "MTLRenderCommandEncoder::setThreadgroupMemoryLength");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useResource,
                               "MTLRenderCommandEncoder::useResource");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useResource_stages,
                               "MTLRenderCommandEncoder::useResource");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useResources,
                               "MTLRenderCommandEncoder::useResources");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useResources_stages,
                               "MTLRenderCommandEncoder::useResources");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useHeap, "MTLRenderCommandEncoder::useHeap");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useHeap_stages,
                               "MTLRenderCommandEncoder::useHeap");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useHeaps,
                               "MTLRenderCommandEncoder::useHeaps");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_useHeaps_stages,
                               "MTLRenderCommandEncoder::useHeaps");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_executeCommandsInBuffer,
                               "MTLRenderCommandEncoder::executeCommandsInBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_executeCommandsInBuffer_indirect,
                               "MTLRenderCommandEncoder::executeCommandsInBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_memoryBarrierWithScope,
                               "MTLRenderCommandEncoder::memoryBarrierWithScope");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_memoryBarrierWithResources,
                               "MTLRenderCommandEncoder::memoryBarrierWithResources");
    STRINGISE_ENUM_CLASS_NAMED(MTLRenderCommandEncoder_sampleCountersInBuffer,
                               "MTLRenderCommandEncoder::sampleCountersInBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_setPurgeableState, "MTLBuffer::setPurgeableState");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_makeAliasable, "MTLBuffer::makeAliasable");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_contents, "MTLBuffer::contents");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_didModifyRange, "MTLBuffer::didModifyRange");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_newTextureWithDescriptor,
                               "MTLBuffer::newTextureWithDescriptor");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_addDebugMarker, "MTLBuffer::addDebugMarker");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_removeAllDebugMarkers, "MTLBuffer::removeAllDebugMarkers");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_remoteStorageBuffer, "MTLBuffer::remoteStorageBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_newRemoteBufferViewForDevice,
                               "MTLBuffer::newRemoteBufferViewForDevice");
    STRINGISE_ENUM_CLASS_NAMED(MTLBuffer_InternalModifyCPUContents,
                               "Internal_MTLBufferModifyCPUContents");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_setLabel, "MTLBlitCommandEncoder::setLabel");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_endEncoding,
                               "MTLBlitCommandEncoder::endEncoding");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_insertDebugSignpost,
                               "MTLBlitCommandEncoder::insertDebugSignpost");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_pushDebugGroup,
                               "MTLBlitCommandEncoder::pushDebugGroup");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_popDebugGroup,
                               "MTLBlitCommandEncoder::popDebugGroup");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_synchronizeResource,
                               "MTLBlitCommandEncoder::synchronizeResource");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_synchronizeTexture,
                               "MTLBlitCommandEncoder::synchronizeTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromBuffer_toBuffer,
                               "MTLBlitCommandEncoder::copyFromBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromBuffer_toTexture,
                               "MTLBlitCommandEncoder::copyFromBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromBuffer_toTexture_options,
                               "MTLBlitCommandEncoder::copyFromBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromTexture_toBuffer,
                               "MTLBlitCommandEncoder::copyFromTexture_toBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromTexture_toBuffer_options,
                               "MTLBlitCommandEncoder::copyFromTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromTexture_toTexture,
                               "MTLBlitCommandEncoder::copyFromTexture_toTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin,
                               "MTLBlitCommandEncoder::copyFromTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_count,
                               "MTLBlitCommandEncoder::copyFromTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_generateMipmapsForTexture,
                               "MTLBlitCommandEncoder::generateMipmapsForTexture");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_fillBuffer,
                               "MTLBlitCommandEncoder::fillBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_updateFence,
                               "MTLBlitCommandEncoder::updateFence");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_waitForFence,
                               "MTLBlitCommandEncoder::waitForFence");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_getTextureAccessCounters,
                               "MTLBlitCommandEncoder::getTextureAccessCounters");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_resetTextureAccessCounters,
                               "MTLBlitCommandEncoder::resetTextureAccessCounters");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_optimizeContentsForGPUAccess,
                               "MTLBlitCommandEncoder::optimizeContentsForGPUAccess");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_optimizeContentsForGPUAccess_slice_level,
                               "MTLBlitCommandEncoder::optimizeContentsForGPUAccess");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_optimizeContentsForCPUAccess,
                               "MTLBlitCommandEncoder::optimizeContentsForCPUAccess");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_optimizeContentsForCPUAccess_slice_level,
                               "MTLBlitCommandEncoder::optimizeContentsForCPUAccess");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_resetCommandsInBuffer,
                               "MTLBlitCommandEncoder::resetCommandsInBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_copyIndirectCommandBuffer,
                               "MTLBlitCommandEncoder::copyIndirectCommandBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_optimizeIndirectCommandBuffer,
                               "MTLBlitCommandEncoder::optimizeIndirectCommandBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_sampleCountersInBuffer,
                               "MTLBlitCommandEncoder::sampleCountersInBuffer");
    STRINGISE_ENUM_CLASS_NAMED(MTLBlitCommandEncoder_resolveCounters,
                               "MTLBlitCommandEncoder::resolveCounters");
    STRINGISE_ENUM_CLASS_NAMED(Max, "Max Chunk");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::Mutability &el)
{
  BEGIN_ENUM_STRINGISE(MTL::Mutability)
  {
    MTL_STRINGISE_ENUM(MutabilityDefault);
    MTL_STRINGISE_ENUM(MutabilityMutable);
    MTL_STRINGISE_ENUM(MutabilityImmutable);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::PixelFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::PixelFormat)
  {
    MTL_STRINGISE_ENUM(PixelFormatInvalid);
    MTL_STRINGISE_ENUM(PixelFormatA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatR8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Uint);
    MTL_STRINGISE_ENUM(PixelFormatR8Sint);
    MTL_STRINGISE_ENUM(PixelFormatR16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatR16Uint);
    MTL_STRINGISE_ENUM(PixelFormatR16Sint);
    MTL_STRINGISE_ENUM(PixelFormatR16Float);
    MTL_STRINGISE_ENUM(PixelFormatRG8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRG8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatRG8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRG8Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG8Sint);
    MTL_STRINGISE_ENUM(PixelFormatB5G6R5Unorm);
    MTL_STRINGISE_ENUM(PixelFormatA1BGR5Unorm);
    MTL_STRINGISE_ENUM(PixelFormatABGR4Unorm);
    MTL_STRINGISE_ENUM(PixelFormatBGR5A1Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR32Uint);
    MTL_STRINGISE_ENUM(PixelFormatR32Sint);
    MTL_STRINGISE_ENUM(PixelFormatR32Float);
    MTL_STRINGISE_ENUM(PixelFormatRG16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRG16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRG16Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG16Sint);
    MTL_STRINGISE_ENUM(PixelFormatRG16Float);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Sint);
    MTL_STRINGISE_ENUM(PixelFormatBGRA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatBGRA8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatRGB10A2Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGB10A2Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG11B10Float);
    MTL_STRINGISE_ENUM(PixelFormatRGB9E5Float);
    MTL_STRINGISE_ENUM(PixelFormatBGR10A2Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRG32Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG32Sint);
    MTL_STRINGISE_ENUM(PixelFormatRG32Float);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Sint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Float);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Sint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Float);
    MTL_STRINGISE_ENUM(PixelFormatBC1_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC1_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC2_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC2_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC3_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC3_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC4_RUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC4_RSnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC5_RGUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC5_RGSnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC6H_RGBFloat);
    MTL_STRINGISE_ENUM(PixelFormatBC6H_RGBUfloat);
    MTL_STRINGISE_ENUM(PixelFormatBC7_RGBAUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC7_RGBAUnorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_2BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_2BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_4BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_4BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_2BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_2BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_4BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_4BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatEAC_R11Unorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_R11Snorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RG11Unorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RG11Snorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RGBA8);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RGBA8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8A1);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8A1_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_4x4_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x4_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x6_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x6_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x6_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x10_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x10_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x12_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_4x4_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x4_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x6_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x6_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x8_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x6_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x8_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x10_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x10_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x12_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_4x4_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x4_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x6_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x6_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x8_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x6_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x8_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x10_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x10_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x12_HDR);
    MTL_STRINGISE_ENUM(PixelFormatGBGR422);
    MTL_STRINGISE_ENUM(PixelFormatBGRG422);
    MTL_STRINGISE_ENUM(PixelFormatDepth16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatDepth32Float);
    MTL_STRINGISE_ENUM(PixelFormatStencil8);
    MTL_STRINGISE_ENUM(PixelFormatDepth24Unorm_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatDepth32Float_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatX32_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatX24_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatBGRA10_XR);
    MTL_STRINGISE_ENUM(PixelFormatBGRA10_XR_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBGR10_XR);
    MTL_STRINGISE_ENUM(PixelFormatBGR10_XR_sRGB);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::VertexFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::VertexFormat)
  {
    MTL_STRINGISE_ENUM(VertexFormatInvalid);
    MTL_STRINGISE_ENUM(VertexFormatUChar2);
    MTL_STRINGISE_ENUM(VertexFormatUChar3);
    MTL_STRINGISE_ENUM(VertexFormatUChar4);
    MTL_STRINGISE_ENUM(VertexFormatChar2);
    MTL_STRINGISE_ENUM(VertexFormatChar3);
    MTL_STRINGISE_ENUM(VertexFormatChar4);
    MTL_STRINGISE_ENUM(VertexFormatUChar2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUChar3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUChar4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatChar2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatChar3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatChar4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort2);
    MTL_STRINGISE_ENUM(VertexFormatUShort3);
    MTL_STRINGISE_ENUM(VertexFormatUShort4);
    MTL_STRINGISE_ENUM(VertexFormatShort2);
    MTL_STRINGISE_ENUM(VertexFormatShort3);
    MTL_STRINGISE_ENUM(VertexFormatShort4);
    MTL_STRINGISE_ENUM(VertexFormatUShort2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatShort2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatShort3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatShort4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatHalf2);
    MTL_STRINGISE_ENUM(VertexFormatHalf3);
    MTL_STRINGISE_ENUM(VertexFormatHalf4);
    MTL_STRINGISE_ENUM(VertexFormatFloat);
    MTL_STRINGISE_ENUM(VertexFormatFloat2);
    MTL_STRINGISE_ENUM(VertexFormatFloat3);
    MTL_STRINGISE_ENUM(VertexFormatFloat4);
    MTL_STRINGISE_ENUM(VertexFormatInt);
    MTL_STRINGISE_ENUM(VertexFormatInt2);
    MTL_STRINGISE_ENUM(VertexFormatInt3);
    MTL_STRINGISE_ENUM(VertexFormatInt4);
    MTL_STRINGISE_ENUM(VertexFormatUInt);
    MTL_STRINGISE_ENUM(VertexFormatUInt2);
    MTL_STRINGISE_ENUM(VertexFormatUInt3);
    MTL_STRINGISE_ENUM(VertexFormatUInt4);
    MTL_STRINGISE_ENUM(VertexFormatInt1010102Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUInt1010102Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUChar4Normalized_BGRA);
    MTL_STRINGISE_ENUM(VertexFormatUChar);
    MTL_STRINGISE_ENUM(VertexFormatChar);
    MTL_STRINGISE_ENUM(VertexFormatUCharNormalized);
    MTL_STRINGISE_ENUM(VertexFormatCharNormalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort);
    MTL_STRINGISE_ENUM(VertexFormatShort);
    MTL_STRINGISE_ENUM(VertexFormatUShortNormalized);
    MTL_STRINGISE_ENUM(VertexFormatShortNormalized);
    MTL_STRINGISE_ENUM(VertexFormatHalf);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::VertexStepFunction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::VertexStepFunction)
  {
    MTL_STRINGISE_ENUM(VertexStepFunctionConstant);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerVertex);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerInstance);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerPatch);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerPatchControlPoint);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::PrimitiveTopologyClass &el)
{
  BEGIN_ENUM_STRINGISE(MTL::PrimitiveTopologyClass)
  {
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassUnspecified);
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassPoint);
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassLine);
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassTriangle);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::Winding &el)
{
  BEGIN_ENUM_STRINGISE(MTL::Winding)
  {
    MTL_STRINGISE_ENUM(WindingClockwise);
    MTL_STRINGISE_ENUM(WindingCounterClockwise);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationFactorFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationFactorFormat)
  {
    MTL_STRINGISE_ENUM(TessellationFactorFormatHalf);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationControlPointIndexType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationControlPointIndexType)
  {
    MTL_STRINGISE_ENUM(TessellationControlPointIndexTypeNone);
    MTL_STRINGISE_ENUM(TessellationControlPointIndexTypeUInt16);
    MTL_STRINGISE_ENUM(TessellationControlPointIndexTypeUInt32);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationFactorStepFunction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationFactorStepFunction)
  {
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionConstant);
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionPerPatch);
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionPerInstance);
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionPerPatchAndPerInstance);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationPartitionMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationPartitionMode)
  {
    MTL_STRINGISE_ENUM(TessellationPartitionModePow2);
    MTL_STRINGISE_ENUM(TessellationPartitionModeInteger);
    MTL_STRINGISE_ENUM(TessellationPartitionModeFractionalOdd);
    MTL_STRINGISE_ENUM(TessellationPartitionModeFractionalEven);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::CPUCacheMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::CPUCacheMode)
  {
    MTL_STRINGISE_ENUM(CPUCacheModeDefaultCache);
    MTL_STRINGISE_ENUM(CPUCacheModeWriteCombined);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::StorageMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::StorageMode)
  {
    MTL_STRINGISE_ENUM(StorageModeShared);
    MTL_STRINGISE_ENUM(StorageModeManaged);
    MTL_STRINGISE_ENUM(StorageModePrivate);
    MTL_STRINGISE_ENUM(StorageModeMemoryless);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::HazardTrackingMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::HazardTrackingMode)
  {
    MTL_STRINGISE_ENUM(HazardTrackingModeDefault);
    MTL_STRINGISE_ENUM(HazardTrackingModeUntracked);
    MTL_STRINGISE_ENUM(HazardTrackingModeTracked);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::ResourceOptions &el)
{
  uint64_t local = (uint64_t)el;
  rdcstr ret;
  // MTL::ResourceOptions is a combined value containing
  // MTL::CPUCacheMode, MTL::StorageMode, MTL::HazardTrackingMode

  // The same value (0) is used for
  // MTLResourceCPUCacheModeDefaultCache
  // MTLResourceStorageModeShared
  // MTLResourceHazardTrackingModeDefault

  if((el & MTL::ResourceCPUCacheModeWriteCombined) == MTL::ResourceCPUCacheModeWriteCombined)
  {
    local &= ~uint64_t(MTL::ResourceCPUCacheModeWriteCombined);
    ret += " | MTLResourceCPUCacheModeWriteCombined";
  }
  else
  {
    ret += " | MTLResourceCPUCacheModeDefaultCache";
  }

  if((el & MTL::ResourceStorageModeManaged) == MTL::ResourceStorageModeManaged)
  {
    local &= ~uint64_t(MTL::ResourceStorageModeManaged);
    ret += " | MTLResourceStorageModeManaged";
  }
  else if((el & MTL::ResourceStorageModePrivate) == MTL::ResourceStorageModePrivate)
  {
    local &= ~uint64_t(MTL::ResourceStorageModePrivate);
    ret += " | MTLResourceStorageModePrivate";
  }
  else if((el & MTL::ResourceStorageModeMemoryless) == MTL::ResourceStorageModeMemoryless)
  {
    local &= ~uint64_t(MTL::ResourceStorageModeMemoryless);
    ret += " | MTLResourceStorageModeMemoryless";
  }
  else
  {
    ret += " | MTLResourceStorageModeShared";
  }

  if((el & MTL::ResourceHazardTrackingModeUntracked) == MTL::ResourceHazardTrackingModeUntracked)
  {
    local &= ~uint64_t(MTL::ResourceHazardTrackingModeUntracked);
    ret += " | MTLResourceHazardTrackingModeUntracked";
  }
  else if((el & MTL::ResourceHazardTrackingModeTracked) == MTL::ResourceHazardTrackingModeTracked)
  {
    local &= ~uint64_t(MTL::ResourceHazardTrackingModeTracked);
    ret += " | MTLResourceHazardTrackingModeTracked";
  }
  else
  {
    ret += " | MTLResourceHazardTrackingModeDefault";
  }

  if(local)
  {
    ret += " | MTLResourceOptions (" + ToStr((uint32_t)local) + ")";
  }

  ret = ret.substr(3);
  return ret;
}

template <>
rdcstr DoStringise(const MTL::TextureType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TextureType)
  {
    MTL_STRINGISE_ENUM(TextureType1D);
    MTL_STRINGISE_ENUM(TextureType1DArray);
    MTL_STRINGISE_ENUM(TextureType2D);
    MTL_STRINGISE_ENUM(TextureType2DArray);
    MTL_STRINGISE_ENUM(TextureType2DMultisample);
    MTL_STRINGISE_ENUM(TextureTypeCube);
    MTL_STRINGISE_ENUM(TextureTypeCubeArray);
    MTL_STRINGISE_ENUM(TextureType3D);
    MTL_STRINGISE_ENUM(TextureType2DMultisampleArray);
    MTL_STRINGISE_ENUM(TextureTypeTextureBuffer);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TextureUsage &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::TextureUsage)
  {
    MTL_STRINGISE_BITFIELD_VALUE(TextureUsageUnknown);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageShaderRead);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageShaderWrite);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageRenderTarget);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsagePixelFormatView);
  }
  END_BITFIELD_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TextureSwizzle &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TextureSwizzle)
  {
    MTL_STRINGISE_ENUM(TextureSwizzleZero);
    MTL_STRINGISE_ENUM(TextureSwizzleOne);
    MTL_STRINGISE_ENUM(TextureSwizzleRed);
    MTL_STRINGISE_ENUM(TextureSwizzleGreen);
    MTL_STRINGISE_ENUM(TextureSwizzleBlue);
    MTL_STRINGISE_ENUM(TextureSwizzleAlpha);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::PrimitiveType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::PrimitiveType)
  {
    MTL_STRINGISE_ENUM(PrimitiveTypePoint);
    MTL_STRINGISE_ENUM(PrimitiveTypeLine);
    MTL_STRINGISE_ENUM(PrimitiveTypeLineStrip);
    MTL_STRINGISE_ENUM(PrimitiveTypeTriangle);
    MTL_STRINGISE_ENUM(PrimitiveTypeTriangleStrip);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::LoadAction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::LoadAction)
  {
    MTL_STRINGISE_ENUM(LoadActionDontCare);
    MTL_STRINGISE_ENUM(LoadActionLoad);
    MTL_STRINGISE_ENUM(LoadActionClear);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::StoreAction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::StoreAction)
  {
    MTL_STRINGISE_ENUM(StoreActionDontCare);
    MTL_STRINGISE_ENUM(StoreActionStore);
    MTL_STRINGISE_ENUM(StoreActionMultisampleResolve);
    MTL_STRINGISE_ENUM(StoreActionStoreAndMultisampleResolve);
    MTL_STRINGISE_ENUM(StoreActionUnknown);
    MTL_STRINGISE_ENUM(StoreActionCustomSampleDepthStore);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::StoreActionOptions &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::StoreActionOptions)
  {
    MTL_STRINGISE_BITFIELD_VALUE(StoreActionOptionNone);
    MTL_STRINGISE_BITFIELD_BIT(StoreActionOptionCustomSamplePositions);
  }
  END_BITFIELD_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::BlendFactor &el)
{
  BEGIN_ENUM_STRINGISE(MTL::BlendFactor)
  {
    MTL_STRINGISE_ENUM(BlendFactorZero);
    MTL_STRINGISE_ENUM(BlendFactorOne);
    MTL_STRINGISE_ENUM(BlendFactorSourceColor);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSourceColor);
    MTL_STRINGISE_ENUM(BlendFactorSourceAlpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSourceAlpha);
    MTL_STRINGISE_ENUM(BlendFactorDestinationColor);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusDestinationColor);
    MTL_STRINGISE_ENUM(BlendFactorDestinationAlpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusDestinationAlpha);
    MTL_STRINGISE_ENUM(BlendFactorSourceAlphaSaturated);
    MTL_STRINGISE_ENUM(BlendFactorBlendColor);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusBlendColor);
    MTL_STRINGISE_ENUM(BlendFactorBlendAlpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusBlendAlpha);
    MTL_STRINGISE_ENUM(BlendFactorSource1Color);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSource1Color);
    MTL_STRINGISE_ENUM(BlendFactorSource1Alpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSource1Alpha);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::BlendOperation &el)
{
  BEGIN_ENUM_STRINGISE(MTL::BlendOperation)
  {
    MTL_STRINGISE_ENUM(BlendOperationAdd);
    MTL_STRINGISE_ENUM(BlendOperationSubtract);
    MTL_STRINGISE_ENUM(BlendOperationReverseSubtract);
    MTL_STRINGISE_ENUM(BlendOperationMin);
    MTL_STRINGISE_ENUM(BlendOperationMax);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::ColorWriteMask &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::ColorWriteMask)
  {
    MTL_STRINGISE_BITFIELD_VALUE(ColorWriteMaskNone);
    MTL_STRINGISE_BITFIELD_VALUE(ColorWriteMaskAll);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskAlpha);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskBlue);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskGreen);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskRed);
  }
  END_BITFIELD_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::MultisampleDepthResolveFilter &el)
{
  BEGIN_ENUM_STRINGISE(MTL::MultisampleDepthResolveFilter)
  {
    MTL_STRINGISE_ENUM(MultisampleDepthResolveFilterSample0);
    MTL_STRINGISE_ENUM(MultisampleDepthResolveFilterMin);
    MTL_STRINGISE_ENUM(MultisampleDepthResolveFilterMax);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::MultisampleStencilResolveFilter &el)
{
  BEGIN_ENUM_STRINGISE(MTL::MultisampleStencilResolveFilter)
  {
    MTL_STRINGISE_ENUM(MultisampleStencilResolveFilterSample0);
    MTL_STRINGISE_ENUM(MultisampleStencilResolveFilterDepthResolvedSample);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::BlitOption &el)
{
  BEGIN_ENUM_STRINGISE(MTL::BlitOption)
  {
    MTL_STRINGISE_ENUM(BlitOptionNone);
    MTL_STRINGISE_ENUM(BlitOptionDepthFromDepthStencil);
    MTL_STRINGISE_ENUM(BlitOptionRowLinearPVRTC);
    MTL_STRINGISE_ENUM(BlitOptionStencilFromDepthStencil);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::DeviceLocation &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::DeviceLocation)
  {
    MTL_STRINGISE_BITFIELD_BIT(DeviceLocationBuiltIn);
    MTL_STRINGISE_BITFIELD_BIT(DeviceLocationSlot);
    MTL_STRINGISE_BITFIELD_BIT(DeviceLocationExternal);
    MTL_STRINGISE_BITFIELD_BIT(DeviceLocationUnspecified);
  }
  END_BITFIELD_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::ArgumentBuffersTier &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::ArgumentBuffersTier)
  {
    MTL_STRINGISE_BITFIELD_BIT(ArgumentBuffersTier1);
    MTL_STRINGISE_BITFIELD_BIT(ArgumentBuffersTier2);
  }
  END_BITFIELD_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::DepthClipMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::DepthClipMode);
  {
    MTL_STRINGISE_ENUM(DepthClipModeClip);
    MTL_STRINGISE_ENUM(DepthClipModeClamp);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MTL::TriangleFillMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TriangleFillMode);
  {
    MTL_STRINGISE_ENUM(TriangleFillModeFill);
    MTL_STRINGISE_ENUM(TriangleFillModeLines);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MTL::CullMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::CullMode);
  {
    MTL_STRINGISE_ENUM(CullModeNone);
    MTL_STRINGISE_ENUM(CullModeFront);
    MTL_STRINGISE_ENUM(CullModeBack);
  }
  END_ENUM_STRINGISE();
};

template <>
rdcstr DoStringise(const MTL::IndexType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::IndexType)
  {
    MTL_STRINGISE_ENUM(IndexTypeUInt16);
    MTL_STRINGISE_ENUM(IndexTypeUInt32);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::AttributeFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::AttributeFormat)
  {
    MTL_STRINGISE_ENUM(AttributeFormatInvalid);
    MTL_STRINGISE_ENUM(AttributeFormatUChar2);
    MTL_STRINGISE_ENUM(AttributeFormatUChar3);
    MTL_STRINGISE_ENUM(AttributeFormatUChar4);
    MTL_STRINGISE_ENUM(AttributeFormatChar2);
    MTL_STRINGISE_ENUM(AttributeFormatChar3);
    MTL_STRINGISE_ENUM(AttributeFormatChar4);
    MTL_STRINGISE_ENUM(AttributeFormatUChar2Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUChar3Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUChar4Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatChar2Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatChar3Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatChar4Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUShort2);
    MTL_STRINGISE_ENUM(AttributeFormatUShort3);
    MTL_STRINGISE_ENUM(AttributeFormatUShort4);
    MTL_STRINGISE_ENUM(AttributeFormatShort2);
    MTL_STRINGISE_ENUM(AttributeFormatShort3);
    MTL_STRINGISE_ENUM(AttributeFormatShort4);
    MTL_STRINGISE_ENUM(AttributeFormatUShort2Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUShort3Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUShort4Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatShort2Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatShort3Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatShort4Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatHalf2);
    MTL_STRINGISE_ENUM(AttributeFormatHalf3);
    MTL_STRINGISE_ENUM(AttributeFormatHalf4);
    MTL_STRINGISE_ENUM(AttributeFormatFloat);
    MTL_STRINGISE_ENUM(AttributeFormatFloat2);
    MTL_STRINGISE_ENUM(AttributeFormatFloat3);
    MTL_STRINGISE_ENUM(AttributeFormatFloat4);
    MTL_STRINGISE_ENUM(AttributeFormatInt);
    MTL_STRINGISE_ENUM(AttributeFormatInt2);
    MTL_STRINGISE_ENUM(AttributeFormatInt3);
    MTL_STRINGISE_ENUM(AttributeFormatInt4);
    MTL_STRINGISE_ENUM(AttributeFormatUInt);
    MTL_STRINGISE_ENUM(AttributeFormatUInt2);
    MTL_STRINGISE_ENUM(AttributeFormatUInt3);
    MTL_STRINGISE_ENUM(AttributeFormatUInt4);
    MTL_STRINGISE_ENUM(AttributeFormatInt1010102Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUInt1010102Normalized);
    MTL_STRINGISE_ENUM(AttributeFormatUChar4Normalized_BGRA);
    MTL_STRINGISE_ENUM(AttributeFormatUChar);
    MTL_STRINGISE_ENUM(AttributeFormatChar);
    MTL_STRINGISE_ENUM(AttributeFormatUCharNormalized);
    MTL_STRINGISE_ENUM(AttributeFormatCharNormalized);
    MTL_STRINGISE_ENUM(AttributeFormatUShort);
    MTL_STRINGISE_ENUM(AttributeFormatShort);
    MTL_STRINGISE_ENUM(AttributeFormatUShortNormalized);
    MTL_STRINGISE_ENUM(AttributeFormatShortNormalized);
    MTL_STRINGISE_ENUM(AttributeFormatHalf);
    // TODO: When metal-cpp is updated to SDK 14.x
    // MTL_STRINGISE_ENUM(AttributeFormatFloatRG11B10);
    // MTL_STRINGISE_ENUM(AttributeFormatFloatRGB9E5);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::StepFunction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::StepFunction)
  {
    MTL_STRINGISE_ENUM(StepFunctionConstant);
    MTL_STRINGISE_ENUM(StepFunctionPerVertex);
    MTL_STRINGISE_ENUM(StepFunctionPerInstance);
    MTL_STRINGISE_ENUM(StepFunctionPerPatch);
    MTL_STRINGISE_ENUM(StepFunctionPerPatchControlPoint);
    MTL_STRINGISE_ENUM(StepFunctionThreadPositionInGridX);
    MTL_STRINGISE_ENUM(StepFunctionThreadPositionInGridY);
    MTL_STRINGISE_ENUM(StepFunctionThreadPositionInGridXIndexed);
    MTL_STRINGISE_ENUM(StepFunctionThreadPositionInGridYIndexed);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::DispatchType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::DispatchType)
  {
    MTL_STRINGISE_ENUM(DispatchTypeSerial);
    MTL_STRINGISE_ENUM(DispatchTypeConcurrent);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MetalResourceType &el)
{
  RDCCOMPILE_ASSERT((uint32_t)MetalResourceType::eResMax == 11, "MetalResourceType changed");
  BEGIN_ENUM_STRINGISE(MetalResourceType);
  {
    STRINGISE_ENUM(eResUnknown);
    STRINGISE_ENUM(eResBuffer);
    STRINGISE_ENUM(eResCommandBuffer);
    STRINGISE_ENUM(eResCommandQueue);
    STRINGISE_ENUM(eResDevice);
    STRINGISE_ENUM(eResLibrary);
    STRINGISE_ENUM(eResFunction);
    STRINGISE_ENUM(eResRenderPipelineState);
    STRINGISE_ENUM(eResTexture);
    STRINGISE_ENUM(eResRenderCommandEncoder);
    STRINGISE_ENUM(eResBlitCommandEncoder);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MetalCmdBufferStatus &el)
{
  BEGIN_ENUM_STRINGISE(MetalCmdBufferStatus)
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(Enqueued);
    STRINGISE_ENUM_CLASS(Committed);
    STRINGISE_ENUM_CLASS(Submitted);
  }
  END_ENUM_STRINGISE()
}
