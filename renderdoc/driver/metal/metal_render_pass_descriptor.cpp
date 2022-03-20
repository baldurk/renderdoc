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

#include "metal_common.h"
#include "metal_manager.h"
#include "metal_resources.h"

// MTLRenderPassAttachmentDescriptor
// id <MTLTexture> texture;
// NSUInteger level;
// NSUInteger slice;
// NSUInteger depthPlane;
// id <MTLTexture> resolveTexture;
// NSUInteger resolveLevel;
// NSUInteger resolveSlice;
// NSUInteger resolveDepthPlane;
// MTLLoadAction loadAction;
// MTLStoreAction storeAction;
// MTLStoreActionOptions storeActionOptions;

// MTLRenderPassColorAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
// MTLClearColor clearColor;

// MTLRenderPassDepthAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
// double clearDepth;
// MTLMultisampleDepthResolveFilter depthResolveFilter;

// MTLRenderPassStencilAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
// uint32_t clearStencil;
// MTLMultisampleStencilResolveFilter stencilResolveFilter;

// MTLRasterizationRateMap ?????
// MTLRenderPassSampleBufferAttachmentDescriptorArray ???

// MTLRenderPassDescriptor
// MTLRenderPassColorAttachmentDescriptorArray
// MTLRenderPassDepthAttachmentDescriptor
// MTLRenderPassStencilAttachmentDescriptor
// id<MTLBuffer> visibilityResultBuffer
// NSUInteger renderTargetArrayLength
// NSUInteger imageblockSampleLength
// NSUInteger threadgroupMemoryLength
// NSUInteger tileWidth
// NSUInteger tileHeight
// NSUInteger defaultRasterSampleCount
// NSUInteger renderTargetWidth
// NSUInteger renderTargetHeight
// MTLSamplePosition* samplePositions
// MTLRasterizationRateMap
// MTLRenderPassSampleBufferAttachmentDescriptorArray

// MTLRenderPass.h
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPassDescriptor *&el)
{
  if(ser.IsReading())
  {
    el = MTL::RenderPassDescriptor::alloc();
    el = el->init();
  }
  MTL::RenderPassColorAttachmentDescriptor *colorAttachments[MAX_RENDER_PASS_COLOR_ATTACHMENTS];
  MTL::RenderPassDepthAttachmentDescriptor *depthAttachment;
  MTL::RenderPassStencilAttachmentDescriptor *stencilAttachment;
  // TODO: id <MTLBuffer> visibilityResultBuffer;
  NS::UInteger renderTargetArrayLength;
  NS::UInteger imageblockSampleLength;
  NS::UInteger threadgroupMemoryLength;
  NS::UInteger tileWidth;
  NS::UInteger tileHeight;
  NS::UInteger defaultRasterSampleCount;
  NS::UInteger renderTargetWidth;
  NS::UInteger renderTargetHeight;
  // TODO: MTLSamplePosition* samplePositions
  // TODO: id<MTLRasterizationRateMap> rasterizationRateMap;
  // TODO: MTLRenderPassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;

  for(uint32_t i = 0; i < MAX_RENDER_PASS_COLOR_ATTACHMENTS; ++i)
  {
    colorAttachments[i] = el->colorAttachments()->object(i);
  }
  depthAttachment = el->depthAttachment();
  stencilAttachment = el->stencilAttachment();

  if(ser.IsWriting())
  {
    // TODO: id <MTLBuffer> visibilityResultBuffer;
    renderTargetArrayLength = el->renderTargetArrayLength();
    imageblockSampleLength = el->imageblockSampleLength();
    threadgroupMemoryLength = el->threadgroupMemoryLength();
    tileWidth = el->tileWidth();
    tileHeight = el->tileHeight();
    defaultRasterSampleCount = el->defaultRasterSampleCount();
    renderTargetWidth = el->renderTargetWidth();
    renderTargetHeight = el->renderTargetHeight();
    // TODO: getSamplePositions:(MTLSamplePosition *)positions count:(NSUInteger)count;
    // TODO: id<MTLRasterizationRateMap> rasterizationRateMap;
    // TODO: MTLRenderPassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;
  }
  SERIALISE_ELEMENT(colorAttachments);
  SERIALISE_ELEMENT(depthAttachment);
  SERIALISE_ELEMENT(stencilAttachment);
  // TODO: id <MTLBuffer> visibilityResultBuffer;
  SERIALISE_ELEMENT(renderTargetArrayLength);
  SERIALISE_ELEMENT(imageblockSampleLength);
  SERIALISE_ELEMENT(threadgroupMemoryLength);
  SERIALISE_ELEMENT(tileWidth);
  SERIALISE_ELEMENT(tileHeight);
  SERIALISE_ELEMENT(defaultRasterSampleCount);
  SERIALISE_ELEMENT(renderTargetWidth);
  SERIALISE_ELEMENT(renderTargetHeight);
  // TODO: getSamplePositions:(MTLSamplePosition *)positions count:(NSUInteger)count;
  // TODO: id<MTLRasterizationRateMap> rasterizationRateMap;
  // TODO: MTLRenderPassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;

  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    // TODO: id <MTLBuffer> visibilityResultBuffer;
    el->setRenderTargetArrayLength(renderTargetArrayLength);
    el->setImageblockSampleLength(imageblockSampleLength);
    el->setThreadgroupMemoryLength(threadgroupMemoryLength);
    el->setTileWidth(tileWidth);
    el->setTileHeight(tileHeight);
    el->setDefaultRasterSampleCount(defaultRasterSampleCount);
    el->setRenderTargetWidth(renderTargetWidth);
    el->setRenderTargetHeight(renderTargetHeight);
    // TODO: getSamplePositions:(MTLSamplePosition *)positions count:(NSUInteger)count;
    // TODO: id<MTLRasterizationRateMap> rasterizationRateMap;
    // TODO: MTLRenderPassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;
  }
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPassAttachmentDescriptor *&el)
{
  // MTLRenderPassAttachmentDescriptor
  WrappedMTLTexture *texture;
  NS::UInteger level;
  NS::UInteger slice;
  NS::UInteger depthPlane;
  WrappedMTLTexture *resolveTexture;
  NS::UInteger resolveLevel;
  NS::UInteger resolveSlice;
  NS::UInteger resolveDepthPlane;
  MTL::LoadAction loadAction;
  MTL::StoreAction storeAction;
  MTL::StoreActionOptions storeActionOptions;

  if(ser.IsWriting())
  {
    texture = GetWrapped(el->texture());
    level = el->level();
    slice = el->slice();
    depthPlane = el->depthPlane();
    resolveTexture = GetWrapped(el->resolveTexture());
    resolveLevel = el->resolveLevel();
    resolveSlice = el->resolveSlice();
    resolveDepthPlane = el->resolveDepthPlane();
    loadAction = el->loadAction();
    storeAction = el->storeAction();
    storeActionOptions = el->storeActionOptions();
  }

  SERIALISE_ELEMENT(texture);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(slice);
  SERIALISE_ELEMENT(depthPlane);
  SERIALISE_ELEMENT(resolveTexture);
  SERIALISE_ELEMENT(resolveLevel);
  SERIALISE_ELEMENT(resolveSlice);
  SERIALISE_ELEMENT(resolveDepthPlane);
  SERIALISE_ELEMENT(loadAction);
  SERIALISE_ELEMENT(storeAction);
  SERIALISE_ELEMENT(storeActionOptions);

  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    el->setTexture(GetObjCBridge(texture));
    el->setLevel(level);
    el->setSlice(slice);
    el->setDepthPlane(depthPlane);
    el->setResolveTexture(GetObjCBridge(resolveTexture));
    el->setResolveLevel(resolveLevel);
    el->setResolveSlice(resolveSlice);
    el->setResolveDepthPlane(resolveDepthPlane);
    el->setLoadAction(loadAction);
    el->setStoreAction(storeAction);
    el->setStoreActionOptions(storeActionOptions);
  }
}

// MTLRenderPassColorAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
// MTLClearColor clearColor;

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPassColorAttachmentDescriptor *&el)
{
  MTL::RenderPassAttachmentDescriptor *descriptor = el;
  MTL::ClearColor clearColor;

  if(ser.IsWriting())
  {
    clearColor = el->clearColor();
  }

  DoSerialise(ser, descriptor);
  SERIALISE_ELEMENT(clearColor);

  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    el->setClearColor(clearColor);
  }
}

// MTLRenderPassDepthAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
// double clearDepth;
// MTLMultisampleDepthResolveFilter depthResolveFilter;

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPassDepthAttachmentDescriptor *&el)
{
  MTL::RenderPassAttachmentDescriptor *descriptor = el;
  double clearDepth;
  // TODO: MTLMultisampleDepthResolveFilter depthResolveFilter;

  if(ser.IsWriting())
  {
    clearDepth = el->clearDepth();
  }

  DoSerialise(ser, descriptor);
  SERIALISE_ELEMENT(clearDepth);
  // TODO: TODO: MTLMultisampleDepthResolveFilter depthResolveFilter;

  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    el->setClearDepth(clearDepth);
  }
}

// MTLRenderPassStencilAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
// uint32_t clearStencil;
// MTLMultisampleStencilResolveFilter stencilResolveFilter;

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPassStencilAttachmentDescriptor *&el)
{
  MTL::RenderPassAttachmentDescriptor *descriptor = el;
  uint32_t clearStencil;
  // TODO: MTLMultisampleStencilResolveFilter stencilResolveFilter;

  if(ser.IsWriting())
  {
    clearStencil = el->clearStencil();
  }

  DoSerialise(ser, descriptor);
  SERIALISE_ELEMENT(clearStencil);
  // TODO: MTLMultisampleStencilResolveFilter stencilResolveFilter;

  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    el->setClearStencil(clearStencil);
  }
}

INSTANTIATE_SERIALISE_TYPE(MTL::RenderPassDescriptor *);
INSTANTIATE_SERIALISE_TYPE(MTL::RenderPassAttachmentDescriptor *);
INSTANTIATE_SERIALISE_TYPE(MTL::RenderPassColorAttachmentDescriptor *);
INSTANTIATE_SERIALISE_TYPE(MTL::RenderPassDepthAttachmentDescriptor *);
INSTANTIATE_SERIALISE_TYPE(MTL::RenderPassStencilAttachmentDescriptor *);
