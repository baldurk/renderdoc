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

#include "core/core.h"
#include "metal_function.h"
#include "metal_manager.h"
#include "metal_resources.h"

// MTLRenderPipeline.h
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPipelineDescriptor *&el)
{
  if(ser.IsReading())
  {
    RDCASSERT(el == NULL);
    el = MTL::RenderPipelineDescriptor::alloc();
    el = el->init();
  }

  NS::String *label = NULL;
  WrappedMTLFunction *vertexFunction = NULL;
  WrappedMTLFunction *fragmentFunction = NULL;
  // TODO: vertexDescriptor : MTLVertexDescriptor
  // TODO: vertexBuffers : MTLPipelineBufferDescriptorArray *
  // TODO: fragmentBuffers : MTLPipelineBufferDescriptorArray *
  MTL::RenderPipelineColorAttachmentDescriptor *colorAttachments[MAX_RENDER_PASS_COLOR_ATTACHMENTS];
  MTL::PixelFormat depthAttachmentPixelFormat;
  MTL::PixelFormat stencilAttachmentPixelFormat;
  NS::UInteger sampleCount;
  bool alphaToCoverageEnabled = false;
  bool alphaToOneEnabled = false;
  bool rasterizationEnabled = false;
  MTL::PrimitiveTopologyClass inputPrimitiveTopology;
  NS::UInteger rasterSampleCount = 0;
  NS::UInteger maxTessellationFactor = 0;
  bool tessellationFactorScaleEnabled = false;
  MTL::TessellationFactorFormat tessellationFactorFormat;
  MTL::TessellationControlPointIndexType tessellationControlPointIndexType;
  MTL::TessellationFactorStepFunction tessellationFactorStepFunction;
  MTL::Winding tessellationOutputWindingOrder;
  MTL::TessellationPartitionMode tessellationPartitionMode;
  bool supportIndirectCommandBuffers = false;
  NS::UInteger maxVertexAmplificationCount = 0;
  // TODO: binaryArchives : NSArray<id<MTLBinaryArchive>>

  for(uint32_t i = 0; i < MAX_RENDER_PASS_COLOR_ATTACHMENTS; ++i)
  {
    colorAttachments[i] = el->colorAttachments()->object(i);
  }

  if(ser.IsWriting())
  {
    label = el->label();
    vertexFunction = GetWrapped((MTL::Function *)el->vertexFunction());
    fragmentFunction = GetWrapped((MTL::Function *)el->fragmentFunction());
    // TODO: vertexDescriptor : MTLVertexDescriptor
    // TODO: vertexBuffers : MTLPipelineBufferDescriptorArray *
    // TODO: fragmentBuffers : MTLPipelineBufferDescriptorArray *
    // TODO: colorAttachments : MTLRenderPipelineColorAttachmentDescriptorArray *
    depthAttachmentPixelFormat = el->depthAttachmentPixelFormat();
    stencilAttachmentPixelFormat = el->stencilAttachmentPixelFormat();
    sampleCount = el->sampleCount();
    alphaToCoverageEnabled = el->alphaToCoverageEnabled();
    alphaToOneEnabled = el->alphaToOneEnabled();
    rasterizationEnabled = el->rasterizationEnabled();
    inputPrimitiveTopology = el->inputPrimitiveTopology();
    rasterSampleCount = el->rasterSampleCount();
    maxTessellationFactor = el->maxTessellationFactor();
    tessellationFactorScaleEnabled = el->tessellationFactorScaleEnabled();
    tessellationFactorFormat = el->tessellationFactorFormat();
    tessellationControlPointIndexType = el->tessellationControlPointIndexType();
    tessellationFactorStepFunction = el->tessellationFactorStepFunction();
    tessellationOutputWindingOrder = el->tessellationOutputWindingOrder();
    tessellationPartitionMode = el->tessellationPartitionMode();
    supportIndirectCommandBuffers = el->supportIndirectCommandBuffers();
    maxVertexAmplificationCount = el->maxVertexAmplificationCount();
    // TODO: binaryArchives : NSArray<id<MTLBinaryArchive>>
  }
  SERIALISE_ELEMENT(label);
  SERIALISE_ELEMENT(vertexFunction);
  SERIALISE_ELEMENT(fragmentFunction);
  // TODO: vertexDescriptor : MTLVertexDescriptor
  // TODO: vertexBuffers : MTLPipelineBufferDescriptorArray *
  // TODO: fragmentBuffers : MTLPipelineBufferDescriptorArray *
  SERIALISE_ELEMENT(colorAttachments);
  SERIALISE_ELEMENT(depthAttachmentPixelFormat);
  SERIALISE_ELEMENT(stencilAttachmentPixelFormat);
  SERIALISE_ELEMENT(sampleCount);
  SERIALISE_ELEMENT(alphaToCoverageEnabled);
  SERIALISE_ELEMENT(alphaToOneEnabled);
  SERIALISE_ELEMENT(rasterizationEnabled);
  SERIALISE_ELEMENT(inputPrimitiveTopology);
  SERIALISE_ELEMENT(rasterSampleCount);
  SERIALISE_ELEMENT(maxTessellationFactor);
  SERIALISE_ELEMENT(tessellationFactorScaleEnabled);
  SERIALISE_ELEMENT(tessellationFactorFormat);
  SERIALISE_ELEMENT(tessellationControlPointIndexType);
  SERIALISE_ELEMENT(tessellationFactorStepFunction);
  SERIALISE_ELEMENT(tessellationOutputWindingOrder);
  SERIALISE_ELEMENT(tessellationPartitionMode);
  SERIALISE_ELEMENT(supportIndirectCommandBuffers);
  SERIALISE_ELEMENT(maxVertexAmplificationCount);
  // TODO: binaryArchives : NSArray<id<MTLBinaryArchive>>
  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    el->setLabel(label);
    MetalResourceManager *rm = (MetalResourceManager *)ser.GetUserData();

    el->setVertexFunction(GetObjCBridge(vertexFunction));
    el->setFragmentFunction(GetObjCBridge(fragmentFunction));
    // TODO: vertexDescriptor : MTLVertexDescriptor
    // TODO: vertexBuffers : MTLPipelineBufferDescriptorArray *
    // TODO: fragmentBuffers : MTLPipelineBufferDescriptorArray *
    el->setDepthAttachmentPixelFormat(depthAttachmentPixelFormat);
    el->setStencilAttachmentPixelFormat(stencilAttachmentPixelFormat);
    el->setSampleCount(sampleCount);
    el->setAlphaToCoverageEnabled(alphaToCoverageEnabled);
    el->setAlphaToOneEnabled(alphaToOneEnabled);
    el->setRasterizationEnabled(rasterizationEnabled);
    el->setInputPrimitiveTopology(inputPrimitiveTopology);
    el->setRasterSampleCount(rasterSampleCount);
    el->setMaxTessellationFactor(maxTessellationFactor);
    el->setTessellationFactorScaleEnabled(tessellationFactorScaleEnabled);
    el->setTessellationFactorFormat(tessellationFactorFormat);
    el->setTessellationControlPointIndexType(tessellationControlPointIndexType);
    el->setTessellationFactorStepFunction(tessellationFactorStepFunction);
    el->setTessellationOutputWindingOrder(tessellationOutputWindingOrder);
    el->setTessellationPartitionMode(tessellationPartitionMode);
    el->setSupportIndirectCommandBuffers(supportIndirectCommandBuffers);
    el->setMaxVertexAmplificationCount(maxVertexAmplificationCount);
    // TODO: binaryArchives : NSArray<id<MTLBinaryArchive>>
  }
}

// MTLRenderPipelineColorAttachmentDescriptor
// MTLPixelFormat pixelFormat;
// BOOL blendingEnabled;
// MTLBlendFactor sourceRGBBlendFactor;
// MTLBlendFactor destinationRGBBlendFactor;
// MTLBlendOperation rgbBlendOperation;
// MTLBlendFactor sourceAlphaBlendFactor;
// MTLBlendFactor destinationAlphaBlendFactor;
// MTLBlendOperation alphaBlendOperation;
// MTLColorWriteMask writeMask;

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MTL::RenderPipelineColorAttachmentDescriptor *&el)
{
  MTL::PixelFormat pixelFormat;
  bool blendingEnabled;
  MTL::BlendFactor sourceRGBBlendFactor;
  MTL::BlendFactor destinationRGBBlendFactor;
  MTL::BlendOperation rgbBlendOperation;
  MTL::BlendFactor sourceAlphaBlendFactor;
  MTL::BlendFactor destinationAlphaBlendFactor;
  MTL::BlendOperation alphaBlendOperation;
  MTL::ColorWriteMask writeMask;

  if(ser.IsWriting())
  {
    pixelFormat = el->pixelFormat();
    blendingEnabled = el->blendingEnabled();
    sourceRGBBlendFactor = el->sourceRGBBlendFactor();
    destinationRGBBlendFactor = el->destinationRGBBlendFactor();
    rgbBlendOperation = el->rgbBlendOperation();
    sourceAlphaBlendFactor = el->sourceAlphaBlendFactor();
    destinationAlphaBlendFactor = el->destinationAlphaBlendFactor();
    alphaBlendOperation = el->alphaBlendOperation();
    writeMask = el->writeMask();
  }

  SERIALISE_ELEMENT(pixelFormat);
  SERIALISE_ELEMENT(blendingEnabled);
  SERIALISE_ELEMENT(sourceRGBBlendFactor);
  SERIALISE_ELEMENT(destinationRGBBlendFactor);
  SERIALISE_ELEMENT(rgbBlendOperation);
  SERIALISE_ELEMENT(sourceAlphaBlendFactor);
  SERIALISE_ELEMENT(destinationAlphaBlendFactor);
  SERIALISE_ELEMENT(alphaBlendOperation);
  SERIALISE_ELEMENT(writeMask);

  if(ser.IsReading())
  {
    RDCASSERT(el != NULL);
    el->setPixelFormat(pixelFormat);
    el->setBlendingEnabled(blendingEnabled);
    el->setSourceRGBBlendFactor(sourceRGBBlendFactor);
    el->setDestinationRGBBlendFactor(destinationRGBBlendFactor);
    el->setRgbBlendOperation(rgbBlendOperation);
    el->setSourceAlphaBlendFactor(sourceAlphaBlendFactor);
    el->setDestinationAlphaBlendFactor(destinationAlphaBlendFactor);
    el->setAlphaBlendOperation(alphaBlendOperation);
    el->setWriteMask(writeMask);
  }
}

INSTANTIATE_SERIALISE_TYPE(MTL::RenderPipelineDescriptor *);
INSTANTIATE_SERIALISE_TYPE(MTL::RenderPipelineColorAttachmentDescriptor *);
