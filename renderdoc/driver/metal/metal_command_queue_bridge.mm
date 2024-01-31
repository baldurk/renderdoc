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

#include "metal_command_queue.h"
#include "metal_types_bridge.h"

// Bridge for MTLCommandQueue
@implementation ObjCBridgeMTLCommandQueue

// ObjCBridgeMTLCommandQueue specific
- (id<MTLCommandQueue>)real
{
  return id<MTLCommandQueue>(Unwrap(GetWrapped(self)));
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

// Use the real MTLCommandQueue to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLCommandQueue
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLCommandQueue : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLCommandQueue.h

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

- (nullable id<MTLCommandBuffer>)commandBuffer
{
  return id<MTLCommandBuffer>(GetWrapped(self)->commandBuffer());
}

- (nullable id<MTLCommandBuffer>)commandBufferWithDescriptor:(MTLCommandBufferDescriptor *)descriptor
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real commandBufferWithDescriptor:descriptor];
}

- (nullable id<MTLCommandBuffer>)commandBufferWithUnretainedReferences
{
  METAL_NOT_HOOKED();
  return [self.real commandBufferWithUnretainedReferences];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (void)insertDebugCaptureBoundary API_DEPRECATED("Use MTLCaptureScope instead",
                                                  macos(10.11, 10.13), ios(8.0, 11.0))
{
  METAL_NOT_HOOKED();
  return [self.real insertDebugCaptureBoundary];
}
#pragma clang diagnostic pop

@end
