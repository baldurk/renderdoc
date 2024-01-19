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

- (void)setBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
           offsets:(const NSUInteger[__nonnull])offsets
         withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setBuffers:buffers offsets:offsets withRange:range];
}

- (void)setBufferOffset:(NSUInteger)offset
                atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setBufferOffset:offset atIndex:index];
}

- (void)setBytes:(const void *)bytes
          length:(NSUInteger)length
         atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3))
{
  METAL_NOT_HOOKED();
  return [self.real setBytes:bytes length:length atIndex:index];
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

@end
