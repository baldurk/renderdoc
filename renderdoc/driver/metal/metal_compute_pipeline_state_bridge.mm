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

#include "metal_compute_pipeline_state.h"
#include "metal_types_bridge.h"

// Bridge for MTLComputePipelineState
@implementation ObjCBridgeMTLComputePipelineState

// ObjCBridgeMTLComputePipelineState specific
- (id<MTLComputePipelineState>)real
{
  return id<MTLComputePipelineState>(Unwrap(GetWrapped(self)));
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

// Use the real MTLComputePipelineState to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLComputePipelineState
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLComputePipelineState : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLComputePipeline.h

- (nullable NSString *)label
{
  return self.real.label;
}

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (NSUInteger)maxTotalThreadsPerThreadgroup
{
  return self.real.maxTotalThreadsPerThreadgroup;
}

- (NSUInteger)threadExecutionWidth
{
  return self.real.threadExecutionWidth;
}

- (NSUInteger)staticThreadgroupMemoryLength API_AVAILABLE(macos(10.13), macCatalyst(13.1),
                                                          ios(11.0), tvos(11.0))
{
  return self.real.staticThreadgroupMemoryLength;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (MTLResourceID)gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0))
{
  return self.real.gpuResourceID;
}
#endif

- (BOOL)supportIndirectCommandBuffers API_AVAILABLE(macos(10.14), ios(12.0))
{
  return self.real.supportIndirectCommandBuffers;
}

- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)imageblockDimensions
    API_AVAILABLE(macos(11.0), ios(11.0))
{
  METAL_NOT_HOOKED();
  return [self.real imageblockMemoryLengthForDimensions:imageblockDimensions];
}

- (nullable id<MTLFunctionHandle>)functionHandleWithFunction:(id<MTLFunction>)function
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real functionHandleWithFunction:function];
}

- (id<MTLComputePipelineState>)
    newComputePipelineStateWithAdditionalBinaryFunctions:(NSArray<id<MTLFunction>> *_Nonnull)functions
                                                   error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real newComputePipelineStateWithAdditionalBinaryFunctions:functions error:error];
}

- (id<MTLVisibleFunctionTable>)newVisibleFunctionTableWithDescriptor:
    (MTLVisibleFunctionTableDescriptor *_Nonnull)descriptor API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real newVisibleFunctionTableWithDescriptor:descriptor];
}

- (id<MTLIntersectionFunctionTable>)newIntersectionFunctionTableWithDescriptor:
    (MTLIntersectionFunctionTableDescriptor *_Nonnull)descriptor
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIntersectionFunctionTableWithDescriptor:descriptor];
}

@end
