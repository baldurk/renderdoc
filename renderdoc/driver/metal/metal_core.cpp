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

#include "metal_core.h"
#include "serialise/rdcfile.h"
#include "metal_blit_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_device.h"
#include "metal_library.h"
#include "metal_render_command_encoder.h"
#include "metal_replay.h"
#include "metal_texture.h"

WriteSerialiser &WrappedMTLDevice::GetThreadSerialiser()
{
  WriteSerialiser *ser = (WriteSerialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
  if(ser)
    return *ser;

  // slow path, but rare
  ser = new WriteSerialiser(new StreamWriter(1024), Ownership::Stream);

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());
  ser->SetVersion(MetalInitParams::CurrentVersion);

  Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

  {
    SCOPED_LOCK(m_ThreadSerialisersLock);
    m_ThreadSerialisers.push_back(ser);
  }

  return *ser;
}

void WrappedMTLDevice::AddAction(const ActionDescription &a)
{
  METAL_NOT_IMPLEMENTED();
}

void WrappedMTLDevice::AddEvent()
{
  METAL_NOT_IMPLEMENTED();
}

#define METAL_CHUNK_NOT_HANDLED()                               \
  {                                                             \
    RDCERR("MetalChunk::%s not handled", ToStr(chunk).c_str()); \
    return false;                                               \
  }

bool WrappedMTLDevice::ProcessChunk(ReadSerialiser &ser, MetalChunk chunk)
{
  switch(chunk)
  {
    case MetalChunk::MTLCreateSystemDefaultDevice:
      return Serialise_MTLCreateSystemDefaultDevice(ser);
    case MetalChunk::MTLDevice_newCommandQueue: return Serialise_newCommandQueue(ser, NULL);
    case MetalChunk::MTLDevice_newCommandQueueWithMaxCommandBufferCount: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newHeapWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newBufferWithLength:
    case MetalChunk::MTLDevice_newBufferWithBytes:
      return Serialise_newBufferWithBytes(ser, NULL, NULL, 0, MTL::ResourceOptionCPUCacheModeDefault);
    case MetalChunk::MTLDevice_newBufferWithBytesNoCopy: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newDepthStencilStateWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newTextureWithDescriptor:
    case MetalChunk::MTLDevice_newTextureWithDescriptor_iosurface:
    case MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable:
    {
      RDMTL::TextureDescriptor descriptor;
      return Serialise_newTextureWithDescriptor(ser, NULL, descriptor);
    }
    case MetalChunk::MTLDevice_newSharedTextureWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newSharedTextureWithHandle: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newSamplerStateWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newDefaultLibrary: return Serialise_newDefaultLibrary(ser, NULL);
    case MetalChunk::MTLDevice_newDefaultLibraryWithBundle: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newLibraryWithFile: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newLibraryWithURL: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newLibraryWithData: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newLibraryWithSource:
      return Serialise_newLibraryWithSource(ser, NULL, NULL, NULL, NULL);
    case MetalChunk::MTLDevice_newLibraryWithStitchedDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor:
    {
      RDMTL::RenderPipelineDescriptor descriptor;
      return Serialise_newRenderPipelineStateWithDescriptor(ser, NULL, descriptor, NULL);
    }
    case MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_options:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newComputePipelineStateWithFunction: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newComputePipelineStateWithFunction_options:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newComputePipelineStateWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newFence: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newRenderPipelineStateWithTileDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newArgumentEncoderWithArguments: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_supportsRasterizationRateMapWithLayerCount:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newRasterizationRateMapWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newIndirectCommandBufferWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newEvent: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newSharedEvent: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newSharedEventWithHandle: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newCounterSampleBufferWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newDynamicLibrary: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newDynamicLibraryWithURL: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newBinaryArchiveWithDescriptor: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLLibrary_newFunctionWithName:
      return m_DummyReplayLibrary->Serialise_newFunctionWithName(ser, NULL, NULL);
    case MetalChunk::MTLLibrary_newFunctionWithName_constantValues: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLLibrary_newFunctionWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLLibrary_newIntersectionFunctionWithDescriptor: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLFunction_newArgumentEncoderWithBufferIndex: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLCommandQueue_commandBuffer:
      return m_DummyReplayCommandQueue->Serialise_commandBuffer(ser, NULL);
    case MetalChunk::MTLCommandQueue_commandBufferWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_enqueue:
      return m_DummyReplayCommandBuffer->Serialise_enqueue(ser);
    case MetalChunk::MTLCommandBuffer_commit:
      return m_DummyReplayCommandBuffer->Serialise_commit(ser);
    case MetalChunk::MTLCommandBuffer_addScheduledHandler: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_presentDrawable:
      return m_DummyReplayCommandBuffer->Serialise_presentDrawable(ser, NULL);
    case MetalChunk::MTLCommandBuffer_presentDrawable_atTime: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_presentDrawable_afterMinimumDuration:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_waitUntilScheduled: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_addCompletedHandler: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_waitUntilCompleted:
      return m_DummyReplayCommandBuffer->Serialise_waitUntilCompleted(ser);
    case MetalChunk::MTLCommandBuffer_blitCommandEncoder:
      return m_DummyReplayCommandBuffer->Serialise_blitCommandEncoder(ser, NULL);
    case MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor:
    {
      RDMTL::RenderPassDescriptor descriptor;
      return m_DummyReplayCommandBuffer->Serialise_renderCommandEncoderWithDescriptor(ser, NULL,
                                                                                      descriptor);
    }
    case MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_computeCommandEncoder: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_encodeWaitForEvent: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_encodeSignalEvent: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_resourceStateCommandEncoder: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_resourceStateCommandEncoderWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoder:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_pushDebugGroup: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_popDebugGroup: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLTexture_setPurgeableState: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_makeAliasable: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_getBytes: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_getBytes_slice: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_replaceRegion: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_replaceRegion_slice: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_newTextureViewWithPixelFormat: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset_swizzle:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_newSharedTextureHandle: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_remoteStorageTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_newRemoteTextureViewForDevice: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLRenderPipelineState_functionHandleWithFunction: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderPipelineState_newRenderPipelineStateWithAdditionalBinaryFunctions:
      METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLRenderCommandEncoder_endEncoding:
      return m_DummyReplayRenderCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLRenderCommandEncoder_insertDebugSignpost: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_pushDebugGroup: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_popDebugGroup: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setRenderPipelineState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setRenderPipelineState(ser, NULL);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBytes: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBufferOffset: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexBuffers: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexTextures: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState_lodclamp:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates_lodclamp:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTable:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTables:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTable:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTables:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexAccelerationStructure:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setViewport:
    {
      MTL::Viewport viewport;
      return m_DummyReplayRenderCommandEncoder->Serialise_setViewport(ser, viewport);
    }
    case MetalChunk::MTLRenderCommandEncoder_setViewports: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFrontFacingWinding: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVertexAmplificationCount: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setCullMode: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setDepthClipMode: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setDepthBias: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setScissorRect: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setScissorRects: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTriangleFillMode: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBytes: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBufferOffset: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBuffers: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentTexture:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentTexture(ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentTextures: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState_lodclamp:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates_lodclamp:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTable:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTables:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTable:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTables:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setFragmentAccelerationStructure:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setBlendColor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setDepthStencilState: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setStencilReferenceValue: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setStencilFrontReferenceValue:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setVisibilityResultMode: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setColorStoreAction: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setDepthStoreAction: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setStencilStoreAction: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setColorStoreActionOptions: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setDepthStoreActionOptions: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setStencilStoreActionOptions:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives:
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced:
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced_base:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawPrimitives(
          ser, MTL::PrimitiveTypePoint, 0, 0, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_textureBarrier: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_updateFence: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_waitForFence: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTessellationFactorBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTessellationFactorScale: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawPatches: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawPatches_indirect: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches_indirect: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileBytes: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileBufferOffset: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileBuffers: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileTextures: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerState: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerState_lodclamp:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerStates: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerStates_lodclamp:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTable: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTables:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTable:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTables:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setTileAccelerationStructure:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_dispatchThreadsPerTile: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_setThreadgroupMemoryLength: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useResource: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useResource_stages: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useResources: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useResources_stages: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useHeap: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useHeap_stages: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useHeaps: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_useHeaps_stages: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer_indirect:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_memoryBarrierWithScope: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_memoryBarrierWithResources: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLRenderCommandEncoder_sampleCountersInBuffer: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLBuffer_setPurgeableState: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_makeAliasable: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_contents: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_didModifyRange:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyBuffer->Serialise_didModifyRange(ser, range);
    }
    case MetalChunk::MTLBuffer_newTextureWithDescriptor: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_addDebugMarker: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_removeAllDebugMarkers: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_remoteStorageBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_newRemoteBufferViewForDevice: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_InternalModifyCPUContents:
      return m_DummyBuffer->Serialise_InternalModifyCPUContents(ser, 0, 0, NULL);

    case MetalChunk::MTLBlitCommandEncoder_setLabel: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_endEncoding:
      return m_DummyReplayBlitCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLBlitCommandEncoder_insertDebugSignpost: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_pushDebugGroup: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_popDebugGroup: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_synchronizeResource:
      return m_DummyReplayBlitCommandEncoder->Serialise_synchronizeResource(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_synchronizeTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture_options:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer_options:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_count:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_generateMipmapsForTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_fillBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_updateFence: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_waitForFence: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_getTextureAccessCounters: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_resetTextureAccessCounters: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForGPUAccess: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForGPUAccess_slice_level:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForCPUAccess: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForCPUAccess_slice_level:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_resetCommandsInBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_copyIndirectCommandBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_optimizeIndirectCommandBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_sampleCountersInBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_resolveCounters: METAL_CHUNK_NOT_HANDLED();

    // no default to get compile error if a chunk is not handled
    case MetalChunk::Max: break;
  }

  {
    SystemChunk system = (SystemChunk)chunk;
    if(system == SystemChunk::DriverInit)
    {
      MetalInitParams InitParams;
      SERIALISE_ELEMENT(InitParams);

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContentsList)
    {
      // TODO: Create initial contents
      RDCERR("SystemChunk::InitialContentsList not handled");

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContents)
    {
      return Serialise_InitialState(ser, ResourceId(), NULL, NULL);
    }
    else if(system == SystemChunk::CaptureScope)
    {
      return Serialise_CaptureScope(ser);
    }
    else if(system == SystemChunk::CaptureEnd)
    {
      SERIALISE_ELEMENT_LOCAL(PresentedImage, ResourceId()).TypedAs("MTLTexture"_lit);

      SERIALISE_CHECK_READ_ERRORS();

      if(PresentedImage != ResourceId())
        m_LastPresentedImage = PresentedImage;

      if(IsLoading(m_State))
      {
        AddEvent();

        ActionDescription action;
        action.customName = "End of Capture";
        action.flags |= ActionFlags::Present;
        action.copyDestination = m_LastPresentedImage;
        AddAction(action);
      }
      return true;
    }
    else if(system < SystemChunk::FirstDriverChunk)
    {
      RDCERR("Unexpected system chunk in capture data: %u", system);
      ser.SkipCurrentChunk();

      SERIALISE_CHECK_READ_ERRORS();
    }
    else
    {
      RDCERR("Unrecognised Chunk type %d", chunk);
      return false;
    }
  }

  return true;
}

void WrappedMTLDevice::AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix)
{
  ResourceDescription &descr = GetReplay()->GetResourceDesc(id);

  uint64_t num;
  memcpy(&num, &id, sizeof(uint64_t));
  descr.name = defaultNamePrefix + (" " + ToStr(num));
  descr.autogeneratedName = true;
  descr.type = type;
  AddResourceCurChunk(descr);
}

void WrappedMTLDevice::DerivedResource(ResourceId parentLive, ResourceId child)
{
  ResourceId parentId = GetResourceManager()->GetOriginalID(parentLive);

  GetReplay()->GetResourceDesc(parentId).derivedResources.push_back(child);
  GetReplay()->GetResourceDesc(child).parentResources.push_back(parentId);
}

void WrappedMTLDevice::AddResourceCurChunk(ResourceDescription &descr)
{
  descr.initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 1);
}

void WrappedMTLDevice::WaitForGPU()
{
  MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
  mtlCommandBuffer->commit();
  mtlCommandBuffer->waitUntilCompleted();
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  // TODO: serialise image references and states

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

void WrappedMTLDevice::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");
  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    RDCASSERT(m_CaptureCommandBuffersSubmitted.empty());
  }

  m_CaptureTimer.Restart();

  GetResourceManager()->ResetCaptureStartTime();

  m_AppControlledCapture = true;

  FrameDescription frame;
  frame.frameNumber = ~0U;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();
  // TODO: handle tracked memory

  // need to do all this atomically so that no other commands
  // will check to see if they need to mark dirty or
  // mark pending dirty and go into the frame record.
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();
    m_State = CaptureState::ActiveCapturing;
  }

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);

  // TODO: are there other resources that need to be marked as frame referenced
}

void WrappedMTLDevice::EndCaptureFrame(ResourceId backbuffer)
{
  CACHE_THREAD_SERIALISER();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  SERIALISE_ELEMENT_LOCAL(PresentedImage, backbuffer).TypedAs("MTLTexture"_lit);

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

bool WrappedMTLDevice::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  ResourceId bbId;
  WrappedMTLTexture *backBuffer = m_CapturedBackbuffer;
  m_CapturedBackbuffer = NULL;
  if(backBuffer)
  {
    bbId = GetResID(backBuffer);
  }
  if(bbId == ResourceId())
  {
    RDCERR("Invalid Capture backbuffer");
    return false;
  }
  GetResourceManager()->MarkResourceFrameReferenced(bbId, eFrameRef_Read);

  // atomically transition to IDLE
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    EndCaptureFrame(bbId);
    m_State = CaptureState::BackgroundCapturing;
  }

  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    // wait for the GPU to be idle
    for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
    {
      WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
      Unwrap(commandBuffer)->waitUntilCompleted();
      // Remove the reference on the real resource added during commit()
      Unwrap(commandBuffer)->release();
    }

    if(m_CaptureCommandBuffersSubmitted.empty())
      WaitForGPU();
  }

  RenderDoc::FramePixels fp;

  MTL::Texture *mtlBackBuffer = Unwrap(backBuffer);

  // The backbuffer has to be a non-framebufferOnly texture
  // to be able to copy the pixels for the thumbnail
  if(!mtlBackBuffer->framebufferOnly())
  {
    const uint32_t maxSize = 2048;

    MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
    MTL::BlitCommandEncoder *mtlBlitEncoder = mtlCommandBuffer->blitCommandEncoder();

    uint32_t sourceWidth = (uint32_t)mtlBackBuffer->width();
    uint32_t sourceHeight = (uint32_t)mtlBackBuffer->height();
    MTL::Origin sourceOrigin(0, 0, 0);
    MTL::Size sourceSize(sourceWidth, sourceHeight, 1);

    MTL::PixelFormat format = mtlBackBuffer->pixelFormat();
    uint32_t bytesPerRow = GetByteSize(sourceWidth, 1, 1, format, 0);
    NS::UInteger bytesPerImage = sourceHeight * bytesPerRow;

    MTL::Buffer *mtlCpuPixelBuffer =
        Unwrap(this)->newBuffer(bytesPerImage, MTL::ResourceStorageModeShared);

    mtlBlitEncoder->copyFromTexture(mtlBackBuffer, 0, 0, sourceOrigin, sourceSize,
                                    mtlCpuPixelBuffer, 0, bytesPerRow, bytesPerImage);
    mtlBlitEncoder->endEncoding();

    mtlCommandBuffer->commit();
    mtlCommandBuffer->waitUntilCompleted();

    fp.len = (uint32_t)mtlCpuPixelBuffer->length();
    fp.data = new uint8_t[fp.len];
    memcpy(fp.data, mtlCpuPixelBuffer->contents(), fp.len);

    mtlCpuPixelBuffer->release();

    ResourceFormat fmt = MakeResourceFormat(format);
    fp.width = sourceWidth;
    fp.height = sourceHeight;
    fp.pitch = bytesPerRow;
    fp.stride = fmt.compByteWidth * fmt.compCount;
    fp.bpc = fmt.compByteWidth;
    fp.bgra = fmt.BGRAOrder();
    fp.max_width = maxSize;
    fp.pitch_requirement = 8;

    // TODO: handle different resource formats
  }

  RDCFile *rdc =
      RenderDoc::Inst().CreateRDC(RDCDriver::Metal, m_CapturedFrames.back().frameNumber, fp);

  StreamWriter *captureWriter = NULL;

  if(rdc)
  {
    SectionProperties props;

    // Compress with LZ4 so that it's fast
    props.flags = SectionFlags::LZ4Compressed;
    props.version = m_SectionVersion;
    props.type = SectionType::FrameCapture;

    captureWriter = rdc->WriteSection(props);
  }
  else
  {
    captureWriter = new StreamWriter(StreamWriter::InvalidStream);
  }

  uint64_t captureSectionSize = 0;

  {
    WriteSerialiser ser(captureWriter, Ownership::Stream);

    ser.SetChunkMetadataRecording(GetThreadSerialiser().GetChunkMetadataRecording());
    ser.SetUserData(GetResourceManager());

    {
      m_InitParams.Set(Unwrap(this), m_ID);
      SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, m_InitParams.GetSerialiseSize());
      SERIALISE_ELEMENT(m_InitParams);
    }

    RDCDEBUG("Inserting Resource Serialisers");
    GetResourceManager()->InsertReferencedChunks(ser);
    GetResourceManager()->InsertInitialContentsChunks(ser);

    RDCDEBUG("Creating Capture Scope");
    GetResourceManager()->Serialise_InitialContentsNeeded(ser);
    // TODO: memory references

    // need over estimate of chunk size when writing directly to file
    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);
      Serialise_CaptureScope(ser);
    }

    {
      uint64_t maxCaptureBeginChunkSizeInBytes = 16;
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureBegin, maxCaptureBeginChunkSizeInBytes);
      Serialise_BeginCaptureFrame(ser);
    }

    // don't need to lock access to m_CaptureCommandBuffersSubmitted as
    // no longer in active capture (the transition is thread-protected)
    // nothing will be pushed to the vector

    {
      std::map<int64_t, Chunk *> recordlist;
      size_t countCmdBuffers = m_CaptureCommandBuffersSubmitted.size();
      // ensure all command buffer records within the frame even if recorded before
      // serialised order must be preserved
      for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
      {
        size_t prevSize = recordlist.size();
        (void)prevSize;
        record->Insert(recordlist);
      }

      size_t prevSize = recordlist.size();
      (void)prevSize;
      m_FrameCaptureRecord->Insert(recordlist);
      RDCDEBUG("Adding %zu/%zu frame capture chunks to file serialiser",
               recordlist.size() - prevSize, recordlist.size());

      float num = float(recordlist.size());
      float idx = 0.0f;

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      {
        RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
        idx += 1.0f;
        it->second->Write(ser);
      }
    }
    captureSectionSize = captureWriter->GetOffset();
  }

  RDCLOG("Captured Metal frame with %f MB capture section in %f seconds",
         double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  // delete tracked cmd buffers - had to keep them alive until after serialiser flush.
  CaptureClearSubmittedCmdBuffers();

  GetResourceManager()->ResetLastWriteTimes();
  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents

  return true;
}

bool WrappedMTLDevice::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Discarding frame capture.");

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  // atomically transition to IDLE
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    m_State = CaptureState::BackgroundCapturing;
  }

  CaptureClearSubmittedCmdBuffers();

  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents

  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(frameNumber, m_CapturedFrames.back().frameNumber);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
  }
  return true;
}

void WrappedMTLDevice::CaptureCmdBufSubmit(MetalResourceRecord *record)
{
  RDCASSERTEQUAL(record->cmdInfo->status, MetalCmdBufferStatus::Submitted);
  RDCASSERT(IsCaptureMode(m_State));
  WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
  if(IsActiveCapturing(m_State))
  {
    std::unordered_set<ResourceId> refIDs;
    // The record will get deleted at the end of active frame capture
    record->AddRef();
    record->AddReferencedIDs(refIDs);
    // snapshot/detect any CPU modifications to the contents
    // of referenced MTLBuffer with shared storage mode
    for(auto it = refIDs.begin(); it != refIDs.end(); ++it)
    {
      ResourceId id = *it;
      MetalResourceRecord *refRecord = GetResourceManager()->GetResourceRecord(id);
      if(refRecord->m_Type == eResBuffer)
      {
        MetalBufferInfo *bufInfo = refRecord->bufInfo;
        if(bufInfo->storageMode == MTL::StorageModeShared)
        {
          size_t diffStart = 0;
          size_t diffEnd = bufInfo->length;
          bool foundDifference = true;
          if(!bufInfo->baseSnapshot.isEmpty())
          {
            foundDifference = FindDiffRange(bufInfo->data, bufInfo->baseSnapshot.data(),
                                            bufInfo->length, diffStart, diffEnd);
            if(diffEnd <= diffStart)
              foundDifference = false;
          }

          if(foundDifference)
          {
            if(bufInfo->data == NULL)
            {
              RDCERR("Writing buffer memory %s that is NULL", ToStr(id).c_str());
              continue;
            }
            Chunk *chunk = NULL;
            {
              CACHE_THREAD_SERIALISER();
              SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_InternalModifyCPUContents);
              ((WrappedMTLBuffer *)refRecord->m_Resource)
                  ->Serialise_InternalModifyCPUContents(ser, diffStart, diffEnd, bufInfo);
              chunk = scope.Get();
            }
            record->AddChunk(chunk);
          }
        }
      }
    }
    record->MarkResourceFrameReferenced(GetResID(commandBuffer->GetCommandQueue()), eFrameRef_Read);
    // pull in frame refs from this command buffer
    record->AddResourceReferences(GetResourceManager());
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_commit);
      commandBuffer->Serialise_commit(ser);
      chunk = scope.Get();
    }
    record->AddChunk(chunk);
    m_CaptureCommandBuffersSubmitted.push_back(record);
  }
  else
  {
    // Remove the reference on the real resource added during commit()
    Unwrap(commandBuffer)->release();
  }
  if(record->cmdInfo->presented)
  {
    AdvanceFrame();
    Present(record);
  }
  // In background or active capture mode the record reference is incremented in
  // CaptureCmdBufEnqueue
  record->Delete(GetResourceManager());
}

void WrappedMTLDevice::CaptureCmdBufCommit(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  if(cbRecord->cmdInfo->status != MetalCmdBufferStatus::Enqueued)
    CaptureCmdBufEnqueue(cbRecord);

  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Enqueued);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Committed;

  size_t countSubmitted = 0;
  for(MetalResourceRecord *record : m_CaptureCommandBuffersEnqueued)
  {
    if(record->cmdInfo->status == MetalCmdBufferStatus::Committed)
    {
      record->cmdInfo->status = MetalCmdBufferStatus::Submitted;
      ++countSubmitted;
      CaptureCmdBufSubmit(record);
      continue;
    }
    break;
  };
  m_CaptureCommandBuffersEnqueued.erase(0, countSubmitted);
}

void WrappedMTLDevice::CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Unknown);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Enqueued;
  cbRecord->AddRef();
  m_CaptureCommandBuffersEnqueued.push_back(cbRecord);

  RDCDEBUG("Enqueing CommandBufferRecord %s %d", ToStr(cbRecord->GetResourceID()).c_str(),
           m_CaptureCommandBuffersEnqueued.count());
}

void WrappedMTLDevice::AdvanceFrame()
{
  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame
}

void WrappedMTLDevice::FirstFrame()
{
  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(this, NULL));

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

void WrappedMTLDevice::Present(MetalResourceRecord *record)
{
  WrappedMTLTexture *backBuffer = record->cmdInfo->backBuffer;
  {
    SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
    if(m_CapturePotentialBackBuffers.count(backBuffer) == 0)
    {
      RDCERR("Capture ignoring Present called on unknown backbuffer");
      return;
    }
  }

  CA::MetalLayer *outputLayer = record->cmdInfo->outputLayer;
  DeviceOwnedWindow devWnd(this, outputLayer);

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(devWnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Metal, true);

  if(!activeWindow)
    return;

  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
  {
    RDCASSERT(m_CapturedBackbuffer == NULL);
    m_CapturedBackbuffer = backBuffer;
    RenderDoc::Inst().EndFrameCapture(devWnd);
  }

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

void WrappedMTLDevice::CaptureClearSubmittedCmdBuffers()
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
  {
    record->Delete(GetResourceManager());
  }

  m_CaptureCommandBuffersSubmitted.clear();
}

void WrappedMTLDevice::RegisterMetalLayer(CA::MetalLayer *mtlLayer)
{
  SCOPED_LOCK(m_CaptureOutputLayersLock);
  if(m_CaptureOutputLayers.count(mtlLayer) == 0)
  {
    m_CaptureOutputLayers.insert(mtlLayer);
    TrackedCAMetalLayer::Track(mtlLayer, this);

    DeviceOwnedWindow devWnd(this, mtlLayer);
    RenderDoc::Inst().AddFrameCapturer(devWnd, &m_Capturer);
  }
}

void WrappedMTLDevice::UnregisterMetalLayer(CA::MetalLayer *mtlLayer)
{
  SCOPED_LOCK(m_CaptureOutputLayersLock);
  RDCASSERT(m_CaptureOutputLayers.count(mtlLayer));
  m_CaptureOutputLayers.erase(mtlLayer);

  DeviceOwnedWindow devWnd(this, mtlLayer);
  RenderDoc::Inst().RemoveFrameCapturer(devWnd);
}

void WrappedMTLDevice::RegisterDrawableInfo(CA::MetalDrawable *caMtlDrawable)
{
  MetalDrawableInfo drawableInfo;
  drawableInfo.mtlLayer = caMtlDrawable->layer();
  drawableInfo.texture = GetWrapped(caMtlDrawable->texture());
  drawableInfo.drawableID = caMtlDrawable->drawableID();
  SCOPED_LOCK(m_CaptureDrawablesLock);
  RDCASSERTEQUAL(m_CaptureDrawableInfos.find(caMtlDrawable), m_CaptureDrawableInfos.end());
  m_CaptureDrawableInfos[caMtlDrawable] = drawableInfo;
}

MetalDrawableInfo WrappedMTLDevice::UnregisterDrawableInfo(MTL::Drawable *mtlDrawable)
{
  MetalDrawableInfo drawableInfo;
  {
    SCOPED_LOCK(m_CaptureDrawablesLock);
    auto it = m_CaptureDrawableInfos.find(mtlDrawable);
    if(it != m_CaptureDrawableInfos.end())
    {
      drawableInfo = it->second;
      m_CaptureDrawableInfos.erase(it);
      return drawableInfo;
    }
  }
  // Not found by pointer fall back and check by drawableID
  NS::UInteger drawableID = mtlDrawable->drawableID();
  for(auto it = m_CaptureDrawableInfos.begin(); it != m_CaptureDrawableInfos.end(); ++it)
  {
    drawableInfo = it->second;
    if(drawableInfo.drawableID == drawableID)
    {
      m_CaptureDrawableInfos.erase(it);
      return drawableInfo;
    }
  }
  drawableInfo.mtlLayer = NULL;
  drawableInfo.texture = NULL;
  return drawableInfo;
}

MetalInitParams::MetalInitParams()
{
  memset(this, 0, sizeof(MetalInitParams));
}

uint64_t MetalInitParams::GetSerialiseSize()
{
  size_t ret = sizeof(*this);
  return (uint64_t)ret;
}

void MetalInitParams::Set(MTL::Device *pRealDevice, ResourceId device)
{
  DeviceID = device;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MetalInitParams &el)
{
  SERIALISE_MEMBER(DeviceID).TypedAs("MTLDevice"_lit);
}

INSTANTIATE_SERIALISE_TYPE(MetalInitParams);
