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

#include "metal_buffer.h"
#include "metal_types_bridge.h"

// Bridge for MTLBuffer
@implementation ObjCBridgeMTLBuffer

// ObjCBridgeMTLBuffer specific
- (id<MTLBuffer>)real
{
  return id<MTLBuffer>(Unwrap(GetWrapped(self)));
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

// Use the real MTLBuffer to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLBuffer
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLResource : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.4.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLResource.h

- (nullable NSString *)label
{
  return self.real.label;
}

- (void)setLabel:value
{
  self.real.label = value;
}

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (MTLCPUCacheMode)cpuCacheMode
{
  return self.real.cpuCacheMode;
}

- (MTLStorageMode)storageMode API_AVAILABLE(macos(10.11), ios(9.0))
{
  return self.real.storageMode;
}

- (MTLHazardTrackingMode)hazardTrackingMode API_AVAILABLE(macos(10.15), ios(13.0))
{
  return self.real.hazardTrackingMode;
}

- (MTLResourceOptions)resourceOptions API_AVAILABLE(macos(10.15), ios(13.0))
{
  return self.real.resourceOptions;
}

- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state
{
  METAL_NOT_HOOKED();
  return [self.real setPurgeableState:state];
}

- (id<MTLHeap>)heap API_AVAILABLE(macos(10.13), ios(10.0))
{
  return self.real.heap;
}

- (NSUInteger)heapOffset API_AVAILABLE(macos(10.15), ios(13.0))
{
  return self.real.heapOffset;
}

- (NSUInteger)allocatedSize API_AVAILABLE(macos(10.13), ios(11.0))
{
  return self.real.allocatedSize;
}

- (void)makeAliasable API_AVAILABLE(macos(10.13), ios(10.0))
{
  METAL_NOT_HOOKED();
  return [self.real makeAliasable];
}

- (BOOL)isAliasable API_AVAILABLE(macos(10.13), ios(10.0))
{
  return [self.real isAliasable];
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_4
- (kern_return_t)setOwnerWithIdentity:(task_id_token_t)task_id_token
    API_AVAILABLE(ios(17.4), watchos(10.4), tvos(17.4), macos(14.4))
{
  METAL_NOT_HOOKED();
  return [self.real setOwnerWithIdentity:task_id_token];
}
#endif

// MTLBuffer : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLBuffer.h

- (NSUInteger)length
{
  return self.real.length;
}

- (void *)contents NS_RETURNS_INNER_POINTER
{
  return GetWrapped(self)->contents();
}

- (void)didModifyRange:(NSRange)range API_AVAILABLE(macos(10.11), macCatalyst(13.0))
                           API_UNAVAILABLE(ios)
{
  return GetWrapped(self)->didModifyRange((NS::Range &)range);
}

- (nullable id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor
                                             offset:(NSUInteger)offset
                                        bytesPerRow:(NSUInteger)bytesPerRow
    API_AVAILABLE(macos(10.13), ios(8.0))
{
  METAL_NOT_HOOKED();
  return [self.real newTextureWithDescriptor:descriptor offset:offset bytesPerRow:bytesPerRow];
}

- (void)addDebugMarker:(NSString *)marker
                 range:(NSRange)range API_AVAILABLE(macos(10.12), ios(10.0))
{
  METAL_NOT_HOOKED();
  return [self.real addDebugMarker:marker range:range];
}

- (void)removeAllDebugMarkers API_AVAILABLE(macos(10.12), ios(10.0))
{
  METAL_NOT_HOOKED();
  return [self.real removeAllDebugMarkers];
}

- (id<MTLBuffer>)remoteStorageBuffer API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  // TODO: This should be the wrapped MTLBuffer
  return self.real.remoteStorageBuffer;
}

- (nullable id<MTLBuffer>)newRemoteBufferViewForDevice:(id<MTLDevice>)device
    API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  METAL_NOT_HOOKED();
  return [self.real newRemoteBufferViewForDevice:device];
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (uint64_t)gpuAddress API_AVAILABLE(macos(13.0), ios(16.0))
{
  return self.real.gpuAddress;
}
#endif

@end
