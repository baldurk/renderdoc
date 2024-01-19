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

#include "metal_compute_command_encoder.h"
#include "metal_types_bridge.h"

// Wrapper for MTLComputeCommandEncoder
@implementation ObjCBridgeMTLComputeCommandEncoder

// ObjCWrappedMTLComputeCommandEncoder specific
- (id<MTLComputeCommandEncoder>)real
{
  return id<MTLComputeCommandEncoder>(Unwrap(GetWrapped(self)));
}

// Silence compiler warning
// error: method possibly missing a [super dealloc] call [-Werror,-Wobjc-missing-super-calls]
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc
{
  DeallocateObjCBridge(GetWrapped(self));
}
#pragma clang diagnostic pop

// Use the real MTLComputeCommandEncoder to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLComputeCommandEncoder
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLCommandEncoder : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLComputeEncoder.h

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (nullable NSString *)label
{
  return self.real.label;
}

- (void)setLabel:value
{
  self.real.label = value;
}

- (void)endEncoding
{
  GetWrapped(self)->endEncoding();
}

- (void)insertDebugSignpost:(NSString *)string
{
  METAL_NOT_HOOKED();
  return [self.real insertDebugSignpost:string];
}

- (void)pushDebugGroup:(NSString *)string
{
  METAL_NOT_HOOKED();
  return [self.real pushDebugGroup:string];
}

- (void)popDebugGroup
{
  METAL_NOT_HOOKED();
  return [self.real popDebugGroup];
}

// MTLComputeCommandEncoder : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLComputeCommandEncoder.h

- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipelineState
{
  GetWrapped(self)->setComputePipelineState(GetWrapped(pipelineState));
}

- (void)setBuffer:(nullable id<MTLBuffer>)buffer
           offset:(NSUInteger)offset
          atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setBuffer:buffer offset:offset atIndex:index];
}

- (void)setBuffer:(id<MTLBuffer>)buffer
           offset:(NSUInteger)offset
  attributeStride:(NSUInteger)stride
          atIndex:(NSUInteger)index
    API_AVAILABLE(macos(14.0), macCatalyst(17.0), ios(17.0))
{
  METAL_NOT_HOOKED();
  return [self.real setBuffer:buffer
                       offset:offset
              attributeStride:stride
                      atIndex:index];
}

- (void)setBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
           offsets:(const NSUInteger[__nonnull])offsets
         withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setBuffers:buffers offsets:offsets withRange:range];
}

- (void)setBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
           offsets:(const NSUInteger[__nonnull])offsets
  attributeStrides:(const NSUInteger [__nonnull])strides
         withRange:(NSRange)range
    API_AVAILABLE(macos(14.0), macCatalyst(17.0), ios(17.0))
{
  METAL_NOT_HOOKED();
  return [self.real setBuffers:buffers
                       offsets:offsets
              attributeStrides:strides
                     withRange:range];
}

- (void)setBufferOffset:(NSUInteger)offset
                atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setBufferOffset:offset atIndex:index];
}

- (void)setBufferOffset:(NSUInteger)offset
        attributeStride:(NSUInteger)stride
                atIndex:(NSUInteger)index
    API_AVAILABLE(macos(14.0), macCatalyst(17.0), ios(17.0))
{
  METAL_NOT_HOOKED();
  return [self.real setBufferOffset:offset
                    attributeStride:stride
                            atIndex:index];
}

- (void)setBytes:(const void *)bytes
          length:(NSUInteger)length
         atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3))
{
  METAL_NOT_HOOKED();
  return [self.real setBytes:bytes length:length atIndex:index];
}

- (void)setBytes:(const void *)bytes
          length:(NSUInteger)length
 attributeStride:(NSUInteger)stride
         atIndex:(NSUInteger)index
    API_AVAILABLE(macos(14.0), macCatalyst(17.0), ios(17.0))
{
  METAL_NOT_HOOKED();
  return [self.real setBytes:bytes
                      length:length
             attributeStride:stride
                     atIndex:index];
}

- (void)setSamplerState:(nullable id<MTLSamplerState>)sampler
                atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setSamplerState:sampler atIndex:index];
}

- (void)setSamplerState:(nullable id<MTLSamplerState>)sampler
            lodMinClamp:(float)lodMinClamp
            lodMaxClamp:(float)lodMaxClamp
                atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setSamplerState:sampler
                        lodMinClamp: lodMinClamp
                        lodMaxClamp: lodMaxClamp
                            atIndex: index];
}

- (void)setSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
               withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setSamplerStates:samplers withRange:range];
}

- (void)setSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
            lodMinClamps:(const float[__nonnull])lodMinClamps
            lodMaxClamps:(const float[__nonnull])lodMaxClamps
               withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setSamplerStates:samplers
                        lodMinClamps:lodMinClamps
                        lodMaxClamps:lodMaxClamps
                           withRange:range];
}

- (void)setTexture:(id<MTLTexture>)texture
           atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setTexture:texture atIndex:index];
}

- (void)setTextures:(const id<MTLTexture> __nullable[__nonnull])textures
          withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setTextures:textures withRange:range];
}

- (void)setThreadgroupMemoryLength:(NSUInteger)length
                           atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setThreadgroupMemoryLength:length atIndex: index];
}

- (void)setVisibleFunctionTable:(id<MTLVisibleFunctionTable>)visibleFunctionTable
                  atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real setVisibleFunctionTable:visibleFunctionTable
                              atBufferIndex:bufferIndex];
}

- (void)setVisibleFunctionTables:(const id<MTLVisibleFunctionTable> __nullable[__nonnull])visibleFunctionTables
                 withBufferRange:(NSRange)range
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real setVisibleFunctionTables:visibleFunctionTables
                             withBufferRange:range];
}

- (void)setIntersectionFunctionTable:(id<MTLIntersectionFunctionTable>)intersectionFunctionTable
                       atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real setIntersectionFunctionTable:intersectionFunctionTable
                                   atBufferIndex:bufferIndex];
}

- (void)setIntersectionFunctionTables:(const id<MTLIntersectionFunctionTable> __nullable[__nonnull])intersectionFunctionTable
                      withBufferRange:(NSRange)range
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real setIntersectionFunctionTables:intersectionFunctionTable
                                  withBufferRange:range];
}

- (void)setAccelerationStructure:(id<MTLAccelerationStructure>)accelerationStructure
                   atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real setAccelerationStructure:accelerationStructure
                               atBufferIndex:bufferIndex];
}

- (void)dispatchThreadgroups:(MTLSize)threadgroupsPerGrid
       threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
  METAL_NOT_HOOKED();
  return [self.real dispatchThreadgroups:threadgroupsPerGrid
                   threadsPerThreadgroup:threadsPerThreadgroup];
}

- (void)dispatchThreads:(MTLSize)threadsPerGrid
  threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
    API_AVAILABLE(macos(10.13), macCatalyst(13.1), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real dispatchThreads:threadsPerGrid
              threadsPerThreadgroup:threadsPerThreadgroup];
}

- (void)dispatchThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)indirectBuffer
                          indirectBufferOffset:(NSUInteger)indirectBufferOffset
                         threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
    API_AVAILABLE(macos(10.11), macCatalyst(13.1), ios(9.0))
{
  METAL_NOT_HOOKED();
  return [self.real dispatchThreadgroupsWithIndirectBuffer:indirectBuffer
                                      indirectBufferOffset:indirectBufferOffset
                                     threadsPerThreadgroup:threadsPerThreadgroup];
}

- (void)useResource:(id<MTLResource>)resource
              usage:(MTLResourceUsage)usage
    API_AVAILABLE(macos(10.13), macCatalyst(13.1), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real useResource:resource usage:usage];
}

- (void)useResources:(const id<MTLResource> __nonnull[__nonnull])resources
               count:(NSUInteger)count
               usage:(MTLResourceUsage)usage
    API_AVAILABLE(macos(10.13), macCatalyst(13.1), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real useResources:resources
                           count:count
                           usage:usage];
}

- (void)useHeap:(id<MTLHeap>)heap API_AVAILABLE(macos(10.13), macCatalyst(13.2), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real useHeap:heap];
}

- (void)useHeaps:(const id<MTLHeap> __nonnull[__nonnull])heaps
           count:(NSUInteger)count
    API_AVAILABLE(macos(10.13), macCatalyst(13.1), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real useHeaps:heaps count:count];
}

- (void)setImageblockWidth:(NSUInteger)width
                    height:(NSUInteger)height
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real setImageblockWidth:width height:height];
}

- (void)setStageInRegion:(MTLRegion)region API_AVAILABLE(macos(10.12), macCatalyst(13.1), ios(10.0))
{
  METAL_NOT_HOOKED();
  return [self.real setStageInRegion:region];
}

- (void)setStageInRegionWithIndirectBuffer:(id<MTLBuffer>)indirectBuffer
                      indirectBufferOffset:(NSUInteger)indirectBufferOffset
    API_AVAILABLE(macos(10.14), macCatalyst(13.1), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real setStageInRegionWithIndirectBuffer:indirectBuffer
                                  indirectBufferOffset:indirectBufferOffset];
}

- (void)memoryBarrierWithScope:(MTLBarrierScope)scope API_AVAILABLE(macos(10.14), macCatalyst(13.1), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real memoryBarrierWithScope:scope];
}

- (void)memoryBarrierWithResources:(const id<MTLResource> __nonnull[__nonnull])resources
                             count:(NSUInteger)count
    API_AVAILABLE(macos(10.14), macCatalyst(13.1), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real memoryBarrierWithResources:resources count:count];
}

- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer
                      withRange:(NSRange)range
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  METAL_NOT_HOOKED();
  return [self.real executeCommandsInBuffer:indirectCommandBuffer withRange:range];
}

- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer
                 indirectBuffer:(id<MTLBuffer>)indirectRangeBuffer
           indirectBufferOffset:(NSUInteger)indirectBufferOffset
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  METAL_NOT_HOOKED();
  return [self.real executeCommandsInBuffer:indirectCommandBuffer
                             indirectBuffer:indirectRangeBuffer
                       indirectBufferOffset:indirectBufferOffset];
}

- (void)updateFence:(id<MTLFence>)fence API_AVAILABLE(macos(10.13), macCatalyst(13.1), ios(10.0))
{
  METAL_NOT_HOOKED();
  return [self.real updateFence:fence];
}

- (void)waitForFence:(id<MTLFence>)fence API_AVAILABLE(macos(10.13), macCatalyst(13.1), ios(10.0))
{
  METAL_NOT_HOOKED();
  return [self.real waitForFence:fence];
}

- (void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                 atSampleIndex:(NSUInteger)sampleIndex
    withBarrier:(BOOL)barrier API_AVAILABLE(macos(10.15), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real sampleCountersInBuffer:sampleBuffer
                             atSampleIndex:sampleIndex
                               withBarrier:barrier];
}

@synthesize dispatchType;

@end
