/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_render_pipeline_state.h"
#include "metal_types_bridge.h"

// Bridge for MTLRenderPipelineState
@implementation ObjCBridgeMTLRenderPipelineState

// ObjCBridgeMTLRenderPipelineState specific
- (id<MTLRenderPipelineState>)real
{
  return id<MTLRenderPipelineState>(Unwrap(self.wrappedCPP));
}

- (void)dealloc
{
  self.wrappedCPP->Dealloc();
  [super dealloc];
}

// Use the real MTLRenderPipelineState to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLRenderPipelineState
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLRenderPipelineState : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPipeline.h

- (nullable NSString *)label
{
  return self.real.label;
}

- (id<MTLDevice>)device
{
  return id<MTLDevice>(self.wrappedCPP->GetObjCBridgeMTLDevice());
}

- (NSUInteger)maxTotalThreadsPerThreadgroup
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  return self.real.maxTotalThreadsPerThreadgroup;
}

- (BOOL)threadgroupSizeMatchesTileSize
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  return self.real.threadgroupSizeMatchesTileSize;
}

- (NSUInteger)imageblockSampleLength
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  return self.real.imageblockSampleLength;
}

- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)imageblockDimensions
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  return self.wrappedCPP->imageblockMemoryLengthForDimensions((MTL::Size &)imageblockDimensions);
}

- (BOOL)supportIndirectCommandBuffers API_AVAILABLE(macos(10.14), ios(12.0))
{
  return self.real.supportIndirectCommandBuffers;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_12_0
- (nullable id<MTLFunctionHandle>)functionHandleWithFunction:(id<MTLFunction>)function
                                                       stage:(MTLRenderStages)stage
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  METAL_NOT_HOOKED();
  return [self.real functionHandleWithFunction:function stage:stage];
}

- (nullable id<MTLVisibleFunctionTable>)newVisibleFunctionTableWithDescriptor:
                                            (MTLVisibleFunctionTableDescriptor *__nonnull)descriptor
                                                                        stage:(MTLRenderStages)stage
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  METAL_NOT_HOOKED();
  return [self.real newVisibleFunctionTableWithDescriptor:descriptor stage:stage];
}

- (nullable id<MTLIntersectionFunctionTable>)
newIntersectionFunctionTableWithDescriptor:(MTLIntersectionFunctionTableDescriptor *_Nonnull)descriptor
                                     stage:(MTLRenderStages)stage
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIntersectionFunctionTableWithDescriptor:descriptor stage:stage];
}

- (nullable id<MTLRenderPipelineState>)
newRenderPipelineStateWithAdditionalBinaryFunctions:
    (nonnull MTLRenderPipelineFunctionsDescriptor *)additionalBinaryFunctions
                                              error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  METAL_NOT_HOOKED();
  return [self.real newRenderPipelineStateWithAdditionalBinaryFunctions:additionalBinaryFunctions
                                                                  error:error];
}
#endif    // #if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_12_0

@end
