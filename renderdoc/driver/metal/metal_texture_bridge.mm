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

#include "metal_texture.h"
#include "metal_types_bridge.h"

// Bridge for MTLTexture
@implementation ObjCBridgeMTLTexture

// ObjCBridgeMTLTexture specific
- (id<MTLTexture>)real
{
  return id<MTLTexture>(Unwrap(GetWrapped(self)));
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

// Use the real MTLTexture to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLTexture
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

// MTLTexture : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLTexture.h

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (id<MTLResource>)rootResource API_DEPRECATED("Use parentTexture or buffer instead",
                                               macos(10.11, 10.12), ios(8.0, 10.0))
{
  return self.real.rootResource;
}
#pragma clang diagnostic pop

- (id<MTLTexture>)parentTexture API_AVAILABLE(macos(10.11), ios(9.0))
{
  return self.real.parentTexture;
}

- (NSUInteger)parentRelativeLevel API_AVAILABLE(macos(10.11), ios(9.0))
{
  return self.real.parentRelativeLevel;
}

- (NSUInteger)parentRelativeSlice API_AVAILABLE(macos(10.11), ios(9.0))
{
  return self.real.parentRelativeSlice;
}

- (id<MTLBuffer>)buffer API_AVAILABLE(macos(10.12), ios(9.0))
{
  return self.real.buffer;
}

- (NSUInteger)bufferOffset API_AVAILABLE(macos(10.12), ios(9.0))
{
  return self.real.bufferOffset;
}

- (NSUInteger)bufferBytesPerRow API_AVAILABLE(macos(10.12), ios(9.0))
{
  return self.real.bufferBytesPerRow;
}

- (IOSurfaceRef)iosurface API_AVAILABLE(macos(10.11), ios(11.0))
{
  return self.real.iosurface;
}

- (NSUInteger)iosurfacePlane API_AVAILABLE(macos(10.11), ios(11.0))
{
  return self.real.iosurfacePlane;
}

- (MTLTextureType)textureType
{
  return self.real.textureType;
}

- (MTLPixelFormat)pixelFormat
{
  return self.real.pixelFormat;
}

- (NSUInteger)width
{
  return self.real.width;
}

- (NSUInteger)height
{
  return self.real.height;
}

- (NSUInteger)depth
{
  return self.real.depth;
}

- (NSUInteger)mipmapLevelCount
{
  return self.real.mipmapLevelCount;
}

- (NSUInteger)sampleCount
{
  return self.real.sampleCount;
}

- (NSUInteger)arrayLength
{
  return self.real.arrayLength;
}

- (MTLTextureUsage)usage
{
  return self.real.usage;
}

- (BOOL)isShareable API_AVAILABLE(macos(10.14), ios(13.0))
{
  return self.real.isShareable;
}

- (BOOL)isFramebufferOnly
{
  return self.real.isFramebufferOnly;
}

- (NSUInteger)firstMipmapInTail API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return self.real.firstMipmapInTail;
}

- (NSUInteger)tailSizeInBytes API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return self.real.tailSizeInBytes;
}

- (BOOL)isSparse API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return self.real.isSparse;
}

- (BOOL)allowGPUOptimizedContents API_AVAILABLE(macos(10.14), ios(12.0))
{
  return self.real.allowGPUOptimizedContents;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_12_5
- (MTLTextureCompressionType)compressionType API_AVAILABLE(macos(12.5), ios(15.0))
{
  return self.real.compressionType;
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (MTLResourceID)gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0))
{
  return self.real.gpuResourceID;
}
#endif

- (void)getBytes:(void *)pixelBytes
      bytesPerRow:(NSUInteger)bytesPerRow
    bytesPerImage:(NSUInteger)bytesPerImage
       fromRegion:(MTLRegion)region
      mipmapLevel:(NSUInteger)level
            slice:(NSUInteger)slice
{
  METAL_NOT_HOOKED();
  [self.real getBytes:pixelBytes
          bytesPerRow:bytesPerRow
        bytesPerImage:bytesPerImage
           fromRegion:region
          mipmapLevel:level
                slice:slice];
}

- (void)replaceRegion:(MTLRegion)region
          mipmapLevel:(NSUInteger)level
                slice:(NSUInteger)slice
            withBytes:(const void *)pixelBytes
          bytesPerRow:(NSUInteger)bytesPerRow
        bytesPerImage:(NSUInteger)bytesPerImage
{
  METAL_NOT_HOOKED();
  [self.real replaceRegion:region
               mipmapLevel:level
                     slice:slice
                 withBytes:pixelBytes
               bytesPerRow:bytesPerRow
             bytesPerImage:bytesPerImage];
}

- (void)getBytes:(void *)pixelBytes
     bytesPerRow:(NSUInteger)bytesPerRow
      fromRegion:(MTLRegion)region
     mipmapLevel:(NSUInteger)level
{
  METAL_NOT_HOOKED();
  [self.real getBytes:pixelBytes bytesPerRow:bytesPerRow fromRegion:region mipmapLevel:level];
}

- (void)replaceRegion:(MTLRegion)region
          mipmapLevel:(NSUInteger)level
            withBytes:(const void *)pixelBytes
          bytesPerRow:(NSUInteger)bytesPerRow
{
  METAL_NOT_HOOKED();
  [self.real replaceRegion:region mipmapLevel:level withBytes:pixelBytes bytesPerRow:bytesPerRow];
}

- (nullable id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat
{
  METAL_NOT_HOOKED();
  return [self.real newTextureViewWithPixelFormat:pixelFormat];
}

- (nullable id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat
                                             textureType:(MTLTextureType)textureType
                                                  levels:(NSRange)levelRange
                                                  slices:(NSRange)sliceRange
    API_AVAILABLE(macos(10.11), ios(9.0))
{
  METAL_NOT_HOOKED();
  return [self.real newTextureViewWithPixelFormat:pixelFormat
                                      textureType:textureType
                                           levels:levelRange
                                           slices:sliceRange];
}

- (nullable MTLSharedTextureHandle *)newSharedTextureHandle API_AVAILABLE(macos(10.14), ios(13.0))
{
  METAL_NOT_HOOKED();
  return [self.real newSharedTextureHandle];
}

- (id<MTLTexture>)remoteStorageTexture API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  METAL_NOT_HOOKED();
  return [self.real remoteStorageTexture];
}

- (nullable id<MTLTexture>)newRemoteTextureViewForDevice:(id<MTLDevice>)device
    API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  METAL_NOT_HOOKED();
  return [self.real newRemoteTextureViewForDevice:device];
}

- (MTLTextureSwizzleChannels)swizzle API_AVAILABLE(macos(10.15), ios(13.0))
{
  return self.real.swizzle;
}

- (nullable id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat
                                             textureType:(MTLTextureType)textureType
                                                  levels:(NSRange)levelRange
                                                  slices:(NSRange)sliceRange
                                                 swizzle:(MTLTextureSwizzleChannels)swizzle
    API_AVAILABLE(macos(10.15), ios(13.0))
{
  METAL_NOT_HOOKED();
  return [self.real newTextureViewWithPixelFormat:pixelFormat
                                      textureType:textureType
                                           levels:levelRange
                                           slices:sliceRange
                                          swizzle:swizzle];
}

@end
