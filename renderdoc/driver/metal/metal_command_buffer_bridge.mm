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

#include "metal_command_buffer.h"
#include "metal_types_bridge.h"

// Bridge for MTLCommandBuffer
@implementation ObjCBridgeMTLCommandBuffer

// ObjCBridgeMTLCommandBuffer specific
- (id<MTLCommandBuffer>)real
{
  return id<MTLCommandBuffer>(Unwrap(GetWrapped(self)));
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

// Use the real MTLCommandBuffer to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLCommandBuffer
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLCommandBuffer : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX13.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLCommandBuffer.h

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (id<MTLCommandQueue>)commandQueue
{
  return id<MTLCommandQueue>(GetWrapped(self)->GetCommandQueue());
}

- (BOOL)retainedReferences
{
  return self.real.retainedReferences;
}

- (MTLCommandBufferErrorOption)errorOptions API_AVAILABLE(macos(11.0), ios(14.0))
{
  return self.real.errorOptions;
}

- (NSString *)label
{
  return self.real.label;
}

- (void)setLabel:value
{
  self.real.label = value;
}

- (CFTimeInterval)kernelStartTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3))
{
  return self.real.kernelStartTime;
}

- (CFTimeInterval)kernelEndTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3))
{
  return self.real.kernelEndTime;
}

- (id<MTLLogContainer>)logs API_AVAILABLE(macos(11.0), ios(14.0))
{
  return self.real.logs;
}

- (CFTimeInterval)GPUStartTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3))
{
  return self.real.GPUStartTime;
}

- (CFTimeInterval)GPUEndTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3))
{
  return self.real.GPUEndTime;
}

- (void)enqueue
{
  GetWrapped(self)->enqueue();
}

- (void)commit
{
  GetWrapped(self)->commit();
}

- (void)addScheduledHandler:(MTLCommandBufferHandler)block
{
  METAL_NOT_HOOKED();
  return [self.real addScheduledHandler:block];
}

- (void)presentDrawable:(id<MTLDrawable>)drawable
{
  GetWrapped(self)->presentDrawable((MTL::Drawable *)drawable);
}

- (void)presentDrawable:(id<MTLDrawable>)drawable atTime:(CFTimeInterval)presentationTime
{
  METAL_NOT_HOOKED();
  return [self.real presentDrawable:drawable atTime:presentationTime];
}

- (void)presentDrawable:(id<MTLDrawable>)drawable
    afterMinimumDuration:(CFTimeInterval)duration
    API_AVAILABLE(macos(10.15.4), ios(10.3), macCatalyst(13.4))
{
  METAL_NOT_HOOKED();
  return [self.real presentDrawable:drawable afterMinimumDuration:duration];
}

- (void)waitUntilScheduled
{
  METAL_NOT_HOOKED();
  return [self.real waitUntilScheduled];
}

- (void)addCompletedHandler:(MTLCommandBufferHandler)block
{
  METAL_NOT_HOOKED();
  return [self.real addCompletedHandler:block];
}

- (void)waitUntilCompleted
{
  GetWrapped(self)->waitUntilCompleted();
}

- (MTLCommandBufferStatus)status
{
  return self.real.status;
}

- (NSError *)error
{
  return self.real.error;
}

- (nullable id<MTLBlitCommandEncoder>)blitCommandEncoder
{
  return id<MTLBlitCommandEncoder>(GetWrapped(self)->blitCommandEncoder());
}

- (nullable id<MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:
    (MTLRenderPassDescriptor *)renderPassDescriptor
{
  RDMTL::RenderPassDescriptor rdDescriptor((MTL::RenderPassDescriptor *)renderPassDescriptor);
  return id<MTLRenderCommandEncoder>(
      GetWrapped(self)->renderCommandEncoderWithDescriptor(rdDescriptor));
}

- (nullable id<MTLComputeCommandEncoder>)computeCommandEncoderWithDescriptor:
    (MTLComputePassDescriptor *)computePassDescriptor API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real computeCommandEncoderWithDescriptor:computePassDescriptor];
}

- (nullable id<MTLBlitCommandEncoder>)blitCommandEncoderWithDescriptor:
    (MTLBlitPassDescriptor *)blitPassDescriptor API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real blitCommandEncoderWithDescriptor:blitPassDescriptor];
}

- (nullable id<MTLComputeCommandEncoder>)computeCommandEncoder
{
  METAL_NOT_HOOKED();
  return [self.real computeCommandEncoder];
}

- (nullable id<MTLComputeCommandEncoder>)computeCommandEncoderWithDispatchType:
    (MTLDispatchType)dispatchType API_AVAILABLE(macos(10.14), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real computeCommandEncoderWithDispatchType:dispatchType];
}

- (void)encodeWaitForEvent:(id<MTLEvent>)event
                     value:(uint64_t)value API_AVAILABLE(macos(10.14), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real encodeWaitForEvent:event value:value];
}

- (void)encodeSignalEvent:(id<MTLEvent>)event
                    value:(uint64_t)value API_AVAILABLE(macos(10.14), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real encodeSignalEvent:event value:value];
}

- (nullable id<MTLParallelRenderCommandEncoder>)parallelRenderCommandEncoderWithDescriptor:
    (MTLRenderPassDescriptor *)renderPassDescriptor
{
  METAL_NOT_HOOKED();
  return [self.real parallelRenderCommandEncoderWithDescriptor:renderPassDescriptor];
}

- (nullable id<MTLResourceStateCommandEncoder>)
    resourceStateCommandEncoder API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  METAL_NOT_HOOKED();
  return [self.real resourceStateCommandEncoder];
}

- (nullable id<MTLResourceStateCommandEncoder>)resourceStateCommandEncoderWithDescriptor:
    (MTLResourceStatePassDescriptor *)resourceStatePassDescriptor
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real resourceStateCommandEncoderWithDescriptor:resourceStatePassDescriptor];
}

- (nullable id<MTLAccelerationStructureCommandEncoder>)
    accelerationStructureCommandEncoder API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real accelerationStructureCommandEncoder];
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (id<MTLAccelerationStructureCommandEncoder>)accelerationStructureCommandEncoderWithDescriptor:
    (MTLAccelerationStructurePassDescriptor *)descriptor API_AVAILABLE(macos(13.0), ios(16.0))
{
  METAL_NOT_HOOKED();
  return [self.real accelerationStructureCommandEncoderWithDescriptor:descriptor];
}
#endif

- (void)pushDebugGroup:(NSString *)string API_AVAILABLE(macos(10.13), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real pushDebugGroup:string];
}

- (void)popDebugGroup API_AVAILABLE(macos(10.13), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real popDebugGroup];
}

@end
