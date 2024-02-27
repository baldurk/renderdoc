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

#include "metal_blit_command_encoder.h"
#include "metal_types_bridge.h"

// Wrapper for MTLBlitCommandEncoder
@implementation ObjCBridgeMTLBlitCommandEncoder

// ObjCWrappedMTLBlitCommandEncoder specific
- (id<MTLBlitCommandEncoder>)real
{
  return id<MTLBlitCommandEncoder>(Unwrap(GetWrapped(self)));
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

// Use the real MTLBlitCommandEncoder to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLBlitCommandEncoder
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLCommandEncoder : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLCommandEncoder.h

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
  GetWrapped(self)->setLabel((NS::String *)value);
}

- (void)endEncoding
{
  GetWrapped(self)->endEncoding();
}

- (void)insertDebugSignpost:(NSString *)string
{
  GetWrapped(self)->insertDebugSignpost((NS::String *)string);
}

- (void)pushDebugGroup:(NSString *)string
{
  GetWrapped(self)->pushDebugGroup((NS::String *)string);
}

- (void)popDebugGroup
{
  GetWrapped(self)->popDebugGroup();
}

// MTLBlitCommandEncoder : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLBlitCommandEncoder.h

- (void)synchronizeResource:(id<MTLResource>)resource API_AVAILABLE(macos(10.11), macCatalyst(13.0))
                                API_UNAVAILABLE(ios)
{
  GetWrapped(self)->synchronizeResource(GetWrapped(resource));
}

- (void)synchronizeTexture:(id<MTLTexture>)texture
                     slice:(NSUInteger)slice
                     level:(NSUInteger)level API_AVAILABLE(macos(10.11), macCatalyst(13.0))
                               API_UNAVAILABLE(ios)
{
  GetWrapped(self)->synchronizeTexture(GetWrapped(texture), slice, level);
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
            sourceSlice:(NSUInteger)sourceSlice
            sourceLevel:(NSUInteger)sourceLevel
           sourceOrigin:(MTLOrigin)sourceOrigin
             sourceSize:(MTLSize)sourceSize
              toTexture:(id<MTLTexture>)destinationTexture
       destinationSlice:(NSUInteger)destinationSlice
       destinationLevel:(NSUInteger)destinationLevel
      destinationOrigin:(MTLOrigin)destinationOrigin
{
  GetWrapped(self)->copyFromTexture(GetWrapped(sourceTexture), sourceSlice, sourceSlice,
                                    (MTL::Origin &)sourceOrigin, (MTL::Size &)sourceSize,
                                    GetWrapped(destinationTexture), destinationSlice,
                                    destinationLevel, (MTL::Origin &)destinationOrigin);
}

- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer
           sourceOffset:(NSUInteger)sourceOffset
      sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
    sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
             sourceSize:(MTLSize)sourceSize
              toTexture:(id<MTLTexture>)destinationTexture
       destinationSlice:(NSUInteger)destinationSlice
       destinationLevel:(NSUInteger)destinationLevel
      destinationOrigin:(MTLOrigin)destinationOrigin
{
  GetWrapped(self)->copyFromBuffer(
      GetWrapped(sourceBuffer), sourceOffset, sourceBytesPerRow, sourceBytesPerImage,
      (MTL::Size &)sourceSize, GetWrapped(destinationTexture), destinationSlice, destinationLevel,
      (MTL::Origin &)destinationOrigin, MTL::BlitOptionNone);
}

- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer
           sourceOffset:(NSUInteger)sourceOffset
      sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
    sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
             sourceSize:(MTLSize)sourceSize
              toTexture:(id<MTLTexture>)destinationTexture
       destinationSlice:(NSUInteger)destinationSlice
       destinationLevel:(NSUInteger)destinationLevel
      destinationOrigin:(MTLOrigin)destinationOrigin
                options:(MTLBlitOption)options API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->copyFromBuffer(
      GetWrapped(sourceBuffer), sourceOffset, sourceBytesPerRow, sourceBytesPerImage,
      (MTL::Size &)sourceSize, GetWrapped(destinationTexture), destinationSlice, destinationLevel,
      (MTL::Origin &)destinationOrigin, (MTL::BlitOption)options);
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
                 sourceSlice:(NSUInteger)sourceSlice
                 sourceLevel:(NSUInteger)sourceLevel
                sourceOrigin:(MTLOrigin)sourceOrigin
                  sourceSize:(MTLSize)sourceSize
                    toBuffer:(id<MTLBuffer>)destinationBuffer
           destinationOffset:(NSUInteger)destinationOffset
      destinationBytesPerRow:(NSUInteger)destinationBytesPerRow
    destinationBytesPerImage:(NSUInteger)destinationBytesPerImage
{
  GetWrapped(self)->copyFromTexture(
      GetWrapped(sourceTexture), sourceSlice, sourceLevel, (MTL::Origin &)sourceOrigin,
      (MTL::Size &)sourceSize, GetWrapped(destinationBuffer), destinationOffset,
      destinationBytesPerRow, destinationBytesPerImage, MTL::BlitOptionNone);
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
                 sourceSlice:(NSUInteger)sourceSlice
                 sourceLevel:(NSUInteger)sourceLevel
                sourceOrigin:(MTLOrigin)sourceOrigin
                  sourceSize:(MTLSize)sourceSize
                    toBuffer:(id<MTLBuffer>)destinationBuffer
           destinationOffset:(NSUInteger)destinationOffset
      destinationBytesPerRow:(NSUInteger)destinationBytesPerRow
    destinationBytesPerImage:(NSUInteger)destinationBytesPerImage
                     options:(MTLBlitOption)options API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->copyFromTexture(
      GetWrapped(sourceTexture), sourceSlice, sourceLevel, (MTL::Origin &)sourceOrigin,
      (MTL::Size &)sourceSize, GetWrapped(destinationBuffer), destinationOffset,
      destinationBytesPerRow, destinationBytesPerImage, (MTL::BlitOption)options);
}

- (void)generateMipmapsForTexture:(id<MTLTexture>)texture
{
  GetWrapped(self)->generateMipmapsForTexture(GetWrapped(texture));
}

- (void)fillBuffer:(id<MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value
{
  GetWrapped(self)->fillBuffer(GetWrapped(buffer), (NS::Range &)range, value);
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
            sourceSlice:(NSUInteger)sourceSlice
            sourceLevel:(NSUInteger)sourceLevel
              toTexture:(id<MTLTexture>)destinationTexture
       destinationSlice:(NSUInteger)destinationSlice
       destinationLevel:(NSUInteger)destinationLevel
             sliceCount:(NSUInteger)sliceCount
             levelCount:(NSUInteger)levelCount API_AVAILABLE(macos(10.15), ios(13.0))
{
  GetWrapped(self)->copyFromTexture(GetWrapped(sourceTexture), sourceSlice, sourceLevel,
                                    GetWrapped(destinationTexture), destinationSlice,
                                    destinationLevel, sliceCount, levelCount);
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
              toTexture:(id<MTLTexture>)destinationTexture API_AVAILABLE(macos(10.15), ios(13.0))
{
  GetWrapped(self)->copyFromTexture(GetWrapped(sourceTexture), GetWrapped(destinationTexture));
}

- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer
          sourceOffset:(NSUInteger)sourceOffset
              toBuffer:(id<MTLBuffer>)destinationBuffer
     destinationOffset:(NSUInteger)destinationOffset
                  size:(NSUInteger)size
{
  GetWrapped(self)->copyFromBuffer(GetWrapped(sourceBuffer), sourceOffset,
                                   GetWrapped(destinationBuffer), destinationOffset, size);
}

- (void)updateFence:(id<MTLFence>)fence API_AVAILABLE(macos(10.13), ios(10.0))
{
  GetWrapped(self)->updateFence(GetWrapped(fence));
}

- (void)waitForFence:(id<MTLFence>)fence API_AVAILABLE(macos(10.13), ios(10.0))
{
  GetWrapped(self)->waitForFence(GetWrapped(fence));
}

- (void)getTextureAccessCounters:(id<MTLTexture>)texture
                          region:(MTLRegion)region
                        mipLevel:(NSUInteger)mipLevel
                           slice:(NSUInteger)slice
                   resetCounters:(BOOL)resetCounters
                  countersBuffer:(id<MTLBuffer>)countersBuffer
            countersBufferOffset:(NSUInteger)countersBufferOffset
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  GetWrapped(self)->getTextureAccessCounters(GetWrapped(texture), (MTL::Region &)region, mipLevel,
                                             slice, resetCounters, GetWrapped(countersBuffer),
                                             countersBufferOffset);
}

- (void)resetTextureAccessCounters:(id<MTLTexture>)texture
                            region:(MTLRegion)region
                          mipLevel:(NSUInteger)mipLevel
                             slice:(NSUInteger)slice
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  GetWrapped(self)->resetTextureAccessCounters(GetWrapped(texture), (MTL::Region &)region, mipLevel,
                                               slice);
}

- (void)optimizeContentsForGPUAccess:(id<MTLTexture>)texture API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->optimizeContentsForGPUAccess(GetWrapped(texture));
}

- (void)optimizeContentsForGPUAccess:(id<MTLTexture>)texture
                               slice:(NSUInteger)slice
                               level:(NSUInteger)level API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->optimizeContentsForGPUAccess(GetWrapped(texture), slice, level);
}

- (void)optimizeContentsForCPUAccess:(id<MTLTexture>)texture API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->optimizeContentsForCPUAccess(GetWrapped(texture));
}

- (void)optimizeContentsForCPUAccess:(id<MTLTexture>)texture
                               slice:(NSUInteger)slice
                               level:(NSUInteger)level API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->optimizeContentsForCPUAccess(GetWrapped(texture), slice, level);
}

- (void)resetCommandsInBuffer:(id<MTLIndirectCommandBuffer>)buffer
                    withRange:(NSRange)range API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->resetCommandsInBuffer(GetWrapped(buffer), (NS::Range &)range);
}

- (void)copyIndirectCommandBuffer:(id<MTLIndirectCommandBuffer>)source
                      sourceRange:(NSRange)sourceRange
                      destination:(id<MTLIndirectCommandBuffer>)destination
                 destinationIndex:(NSUInteger)destinationIndex API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->copyIndirectCommandBuffer(GetWrapped(source), (NS::Range &)sourceRange,
                                              GetWrapped(destination), destinationIndex);
}

- (void)optimizeIndirectCommandBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer
                            withRange:(NSRange)range API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->optimizeIndirectCommandBuffer(GetWrapped(indirectCommandBuffer),
                                                  (NS::Range &)range);
}

- (void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                 atSampleIndex:(NSUInteger)sampleIndex
                   withBarrier:(BOOL)barrier API_AVAILABLE(macos(10.15), ios(14.0))
{
  GetWrapped(self)->sampleCountersInBuffer(GetWrapped(sampleBuffer), sampleIndex, barrier);
}

- (void)resolveCounters:(id<MTLCounterSampleBuffer>)sampleBuffer
                inRange:(NSRange)range
      destinationBuffer:(id<MTLBuffer>)destinationBuffer
      destinationOffset:(NSUInteger)destinationOffset API_AVAILABLE(macos(10.15), ios(14.0))
{
  GetWrapped(self)->resolveCounters(GetWrapped(sampleBuffer), (NS::Range &)range,
                                    GetWrapped(destinationBuffer), destinationOffset);
}

@end
