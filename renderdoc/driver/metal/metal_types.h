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

#pragma once

#include "api/replay/rdcstr.h"
#include "official/metal-cpp.h"
#include "serialise/serialiser.h"

// TODO: use Metal Feature sets to determine these values at capture time
const uint32_t MAX_RENDER_PASS_COLOR_ATTACHMENTS = 8;
const uint32_t MAX_RENDER_PASS_BUFFER_ATTACHMENTS = 31;
const uint32_t MAX_VERTEX_SHADER_ATTRIBUTES = 31;
const uint32_t MAX_RENDER_PASS_SAMPLE_BUFFER_ATTACHMENTS = 4;
const uint32_t MAX_COMPUTE_PASS_BUFFER_ATTACHMENTS = 31;
const uint32_t MAX_COMPUTE_PASS_SAMPLE_BUFFER_ATTACHMENTS = 4;

// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLCounters.h
#ifndef MTLCounterDontSample
#define MTLCounterDontSample ((NS::UInteger)-1)
#endif    // #ifndef MTLCounterDontSample

#define METALCPP_WRAPPED_PROTOCOLS(FUNC) \
  FUNC(Buffer);                          \
  FUNC(CommandBuffer);                   \
  FUNC(CommandQueue);                    \
  FUNC(Device);                          \
  FUNC(Function);                        \
  FUNC(Library);                         \
  FUNC(RenderPipelineState);             \
  FUNC(Texture);                         \
  FUNC(RenderCommandEncoder);            \
  FUNC(BlitCommandEncoder);

// These serialise overloads will fetch the ID during capture, serialise the ID
// directly as-if it were the original type, then on replay load up the resource if available.
#define DECLARE_WRAPPED_TYPE_SERIALISE(CPPTYPE)       \
  class WrappedMTL##CPPTYPE;                          \
  template <>                                         \
  inline rdcliteral TypeName<WrappedMTL##CPPTYPE *>() \
  {                                                   \
    return STRING_LITERAL(STRINGIZE(MTL##CPPTYPE));   \
  }                                                   \
  template <class SerialiserType>                     \
  void DoSerialise(SerialiserType &ser, WrappedMTL##CPPTYPE *&el);

METALCPP_WRAPPED_PROTOCOLS(DECLARE_WRAPPED_TYPE_SERIALISE);
DECLARE_WRAPPED_TYPE_SERIALISE(Resource)
#undef DECLARE_WRAPPED_TYPE_SERIALISE

#define DECLARE_OBJC_HELPERS(CPPTYPE)                           \
  class WrappedMTL##CPPTYPE;                                    \
  inline WrappedMTL##CPPTYPE *GetWrapped(MTL::CPPTYPE *cppType) \
  {                                                             \
    return (WrappedMTL##CPPTYPE *)cppType;                      \
  }                                                             \
  extern void AllocateObjCBridge(WrappedMTL##CPPTYPE *wrapped); \
  extern void DeallocateObjCBridge(WrappedMTL##CPPTYPE *wrapped);

METALCPP_WRAPPED_PROTOCOLS(DECLARE_OBJC_HELPERS)
#undef DECLARE_OBJC_HELPERS

// TODO: Wrapped types that need implementing
#define METALCPP_UNIMPLEMENTED_WRAPPED_PROTOCOLS(FUNC) \
  FUNC(Fence);                                         \
  FUNC(IndirectCommandBuffer);                         \
  FUNC(CounterSampleBuffer);

#define DECLARE_UNIMPLEMENTED_WRAPPED_CPP_HELPERS(CPPTYPE) \
  class WrappedMTL##CPPTYPE;                               \
  inline MTL::CPPTYPE *Unwrap(WrappedMTL##CPPTYPE *obj)    \
  {                                                        \
    return (MTL::CPPTYPE *)obj;                            \
  }
METALCPP_UNIMPLEMENTED_WRAPPED_PROTOCOLS(DECLARE_UNIMPLEMENTED_WRAPPED_CPP_HELPERS)
#undef DECLARE_UNIMPLEMENTED_WRAPPED_CPP_HELPERS

class TrackedCAMetalLayer
{
public:
  TrackedCAMetalLayer() = delete;
  ~TrackedCAMetalLayer() = default;

  static void Track(CA::MetalLayer *mtlLayer, WrappedMTLDevice *device)
  {
    new TrackedCAMetalLayer(mtlLayer, device);
  }
  void StopTracking();

private:
  TrackedCAMetalLayer(CA::MetalLayer *real, WrappedMTLDevice *device);

  void *m_ObjcBridge = NULL;
  WrappedMTLDevice *m_Device = NULL;
  CA::MetalLayer *m_mtlLayer = NULL;
};

#define MTL_DECLARE_REFLECTION_TYPE(TYPE)        \
  template <>                                    \
  inline rdcliteral TypeName<MTL::TYPE>()        \
  {                                              \
    return STRING_LITERAL(STRINGIZE(MTL##TYPE)); \
  }                                              \
  template <class SerialiserType>                \
  void DoSerialise(SerialiserType &ser, MTL::TYPE &el);

MTL_DECLARE_REFLECTION_TYPE(TextureType);
MTL_DECLARE_REFLECTION_TYPE(PixelFormat);
MTL_DECLARE_REFLECTION_TYPE(ResourceOptions);
MTL_DECLARE_REFLECTION_TYPE(CPUCacheMode);
MTL_DECLARE_REFLECTION_TYPE(StorageMode);
MTL_DECLARE_REFLECTION_TYPE(HazardTrackingMode);
MTL_DECLARE_REFLECTION_TYPE(TextureUsage);
MTL_DECLARE_REFLECTION_TYPE(TextureSwizzleChannels);
MTL_DECLARE_REFLECTION_TYPE(TextureSwizzle);
MTL_DECLARE_REFLECTION_TYPE(BlendFactor);
MTL_DECLARE_REFLECTION_TYPE(BlendOperation);
MTL_DECLARE_REFLECTION_TYPE(ColorWriteMask);
MTL_DECLARE_REFLECTION_TYPE(Mutability);
MTL_DECLARE_REFLECTION_TYPE(VertexFormat);
MTL_DECLARE_REFLECTION_TYPE(VertexStepFunction);
MTL_DECLARE_REFLECTION_TYPE(PrimitiveTopologyClass);
MTL_DECLARE_REFLECTION_TYPE(TessellationPartitionMode);
MTL_DECLARE_REFLECTION_TYPE(TessellationFactorFormat);
MTL_DECLARE_REFLECTION_TYPE(TessellationControlPointIndexType);
MTL_DECLARE_REFLECTION_TYPE(TessellationFactorStepFunction);
MTL_DECLARE_REFLECTION_TYPE(Winding);
MTL_DECLARE_REFLECTION_TYPE(PrimitiveType);
MTL_DECLARE_REFLECTION_TYPE(StoreActionOptions);
MTL_DECLARE_REFLECTION_TYPE(LoadAction);
MTL_DECLARE_REFLECTION_TYPE(StoreAction);
MTL_DECLARE_REFLECTION_TYPE(ClearColor);
MTL_DECLARE_REFLECTION_TYPE(Viewport);
MTL_DECLARE_REFLECTION_TYPE(MultisampleDepthResolveFilter);
MTL_DECLARE_REFLECTION_TYPE(MultisampleStencilResolveFilter);
MTL_DECLARE_REFLECTION_TYPE(SamplePosition);
MTL_DECLARE_REFLECTION_TYPE(Origin);
MTL_DECLARE_REFLECTION_TYPE(Region);
MTL_DECLARE_REFLECTION_TYPE(Size);
MTL_DECLARE_REFLECTION_TYPE(BlitOption);
MTL_DECLARE_REFLECTION_TYPE(DeviceLocation);
MTL_DECLARE_REFLECTION_TYPE(ArgumentBuffersTier);
MTL_DECLARE_REFLECTION_TYPE(DepthClipMode);
MTL_DECLARE_REFLECTION_TYPE(TriangleFillMode);
MTL_DECLARE_REFLECTION_TYPE(CullMode);
MTL_DECLARE_REFLECTION_TYPE(IndexType);
MTL_DECLARE_REFLECTION_TYPE(StepFunction);
MTL_DECLARE_REFLECTION_TYPE(AttributeFormat);
MTL_DECLARE_REFLECTION_TYPE(DispatchType);

template <>
inline rdcliteral TypeName<NS::Range>()
{
  return "NSRange"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, NS::Range &el);

namespace RDMTL
{
// MTLTextureDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLTexture.h
struct TextureDescriptor
{
  TextureDescriptor() = default;
  TextureDescriptor(MTL::TextureDescriptor *objc);
  explicit operator MTL::TextureDescriptor *();
  MTL::TextureType textureType = MTL::TextureType2D;
  MTL::PixelFormat pixelFormat = MTL::PixelFormatRGBA8Unorm;
  NS::UInteger width = 1;
  NS::UInteger height = 1;
  NS::UInteger depth = 1;
  NS::UInteger mipmapLevelCount = 1;
  NS::UInteger sampleCount = 1;
  NS::UInteger arrayLength = 1;
  MTL::ResourceOptions resourceOptions = MTL::ResourceStorageModeManaged;
  MTL::CPUCacheMode cpuCacheMode = MTL::CPUCacheModeDefaultCache;
  MTL::StorageMode storageMode = MTL::StorageModeManaged;
  MTL::HazardTrackingMode hazardTrackingMode = MTL::HazardTrackingModeDefault;
  MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
  bool allowGPUOptimizedContents = true;
  MTL::TextureSwizzleChannels swizzle = {MTL::TextureSwizzleRed, MTL::TextureSwizzleGreen,
                                         MTL::TextureSwizzleBlue, MTL::TextureSwizzleAlpha};
};

// MTLRenderPipelineColorAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPipeline.h
struct RenderPipelineColorAttachmentDescriptor
{
  RenderPipelineColorAttachmentDescriptor() = default;
  RenderPipelineColorAttachmentDescriptor(MTL::RenderPipelineColorAttachmentDescriptor *objc);
  void CopyTo(MTL::RenderPipelineColorAttachmentDescriptor *objc);
  MTL::PixelFormat pixelFormat = MTL::PixelFormatInvalid;
  bool blendingEnabled = false;
  MTL::BlendFactor sourceRGBBlendFactor = MTL::BlendFactorOne;
  MTL::BlendFactor destinationRGBBlendFactor = MTL::BlendFactorZero;
  MTL::BlendOperation rgbBlendOperation = MTL::BlendOperationAdd;
  MTL::BlendFactor sourceAlphaBlendFactor = MTL::BlendFactorOne;
  MTL::BlendFactor destinationAlphaBlendFactor = MTL::BlendFactorZero;
  MTL::BlendOperation alphaBlendOperation = MTL::BlendOperationAdd;
  MTL::ColorWriteMask writeMask = MTL::ColorWriteMaskAll;
};

// MTLPipelineBufferDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLPipeline.h
struct PipelineBufferDescriptor
{
  PipelineBufferDescriptor() = default;
  PipelineBufferDescriptor(MTL::PipelineBufferDescriptor *objc);
  void CopyTo(MTL::PipelineBufferDescriptor *objc);
  MTL::Mutability mutability = MTL::MutabilityDefault;
};

// MTLVertexAttributeDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLVertexDescriptor.h
struct VertexAttributeDescriptor
{
  VertexAttributeDescriptor() = default;
  VertexAttributeDescriptor(MTL::VertexAttributeDescriptor *objc);
  void CopyTo(MTL::VertexAttributeDescriptor *objc);
  MTL::VertexFormat format = MTL::VertexFormatInvalid;
  NS::UInteger offset = 0;
  NS::UInteger bufferIndex = 0;
};

// MTLVertexBufferLayoutDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLVertexDescriptor.h
struct VertexBufferLayoutDescriptor
{
  VertexBufferLayoutDescriptor() = default;
  VertexBufferLayoutDescriptor(MTL::VertexBufferLayoutDescriptor *objc);
  void CopyTo(MTL::VertexBufferLayoutDescriptor *objc);
  NS::UInteger stride = 0;
  MTL::VertexStepFunction stepFunction = MTL::VertexStepFunctionPerVertex;
  NS::UInteger stepRate = 1;
};

// MTLVertexDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLVertexDescriptor.h
struct VertexDescriptor
{
  VertexDescriptor() = default;
  VertexDescriptor(MTL::VertexDescriptor *objc);
  void CopyTo(MTL::VertexDescriptor *objc);
  rdcarray<VertexBufferLayoutDescriptor> layouts;
  rdcarray<VertexAttributeDescriptor> attributes;
};

// MTLAttributeDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLStageInputOutputDescriptor.h
struct AttributeDescriptor
{
  AttributeDescriptor() = default;
  AttributeDescriptor(MTL::AttributeDescriptor *objc);
  void CopyTo(MTL::AttributeDescriptor *objc);
  NS::UInteger bufferIndex = 0;
  NS::UInteger offset = 0;
  MTL::AttributeFormat format = MTL::AttributeFormatInvalid;
};

// MTLBufferLayoutDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLStageInputOutputDescriptor.h
struct BufferLayoutDescriptor
{
  BufferLayoutDescriptor() = default;
  BufferLayoutDescriptor(MTL::BufferLayoutDescriptor *objc);
  void CopyTo(MTL::BufferLayoutDescriptor *objc);
  NS::UInteger stride = 0;
  MTL::StepFunction stepFunction = MTL::StepFunctionConstant;
  NS::UInteger stepRate = 0;
};

// MTLStageInputOutputDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLStageInputOutputDescriptor.h
struct StageInputOutputDescriptor
{
  StageInputOutputDescriptor() = default;
  StageInputOutputDescriptor(MTL::StageInputOutputDescriptor *objc);
  void CopyTo(MTL::StageInputOutputDescriptor *objc);
  rdcarray<AttributeDescriptor> attributes;
  rdcarray<BufferLayoutDescriptor> layouts;
  NS::UInteger indexBufferIndex = 0;
  MTL::IndexType indexType = MTL::IndexType::IndexTypeUInt16;
};

// Helper struct for holding MTLLinkedFunctions::groups data
// NSDictionary<NSString*, NSArray<id<MTLFunction>>*> *groups;
struct FunctionGroup
{
  rdcstr callsite;
  rdcarray<WrappedMTLFunction *> functions;
};

// MTLLinkedFunctions : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLLinkedFunctions.h
struct LinkedFunctions
{
  LinkedFunctions() = default;
  LinkedFunctions(MTL::LinkedFunctions *objc);
  void CopyTo(MTL::LinkedFunctions *objc);
  rdcarray<WrappedMTLFunction *> functions;
  rdcarray<WrappedMTLFunction *> binaryFunctions;
  rdcarray<FunctionGroup> groups;
  rdcarray<WrappedMTLFunction *> privateFunctions;
};

// MTLRenderPipelineDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPipeline.h
struct RenderPipelineDescriptor
{
  RenderPipelineDescriptor() = default;
  RenderPipelineDescriptor(MTL::RenderPipelineDescriptor *objc);
  explicit operator MTL::RenderPipelineDescriptor *();
  rdcstr label;
  WrappedMTLFunction *vertexFunction = NULL;
  WrappedMTLFunction *fragmentFunction = NULL;
  VertexDescriptor vertexDescriptor;
  NS::UInteger sampleCount = 1;
  NS::UInteger rasterSampleCount = 1;
  bool alphaToCoverageEnabled = false;
  bool alphaToOneEnabled = false;
  bool rasterizationEnabled = true;
  NS::UInteger maxVertexAmplificationCount = 1;
  rdcarray<RenderPipelineColorAttachmentDescriptor> colorAttachments;
  MTL::PixelFormat depthAttachmentPixelFormat = MTL::PixelFormatInvalid;
  MTL::PixelFormat stencilAttachmentPixelFormat = MTL::PixelFormatInvalid;
  MTL::PrimitiveTopologyClass inputPrimitiveTopology = MTL::PrimitiveTopologyClassUnspecified;
  MTL::TessellationPartitionMode tessellationPartitionMode = MTL::TessellationPartitionModePow2;
  NS::UInteger maxTessellationFactor = 16;
  bool tessellationFactorScaleEnabled = false;
  MTL::TessellationFactorFormat tessellationFactorFormat = MTL::TessellationFactorFormatHalf;
  MTL::TessellationControlPointIndexType tessellationControlPointIndexType =
      MTL::TessellationControlPointIndexTypeNone;
  MTL::TessellationFactorStepFunction tessellationFactorStepFunction =
      MTL::TessellationFactorStepFunctionConstant;
  MTL::Winding tessellationOutputWindingOrder = MTL::WindingClockwise;
  rdcarray<PipelineBufferDescriptor> vertexBuffers;
  rdcarray<PipelineBufferDescriptor> fragmentBuffers;
  bool supportIndirectCommandBuffers = false;
  // TODO: when WrappedMTLBinaryArchive exists
  // rdcarray<WrappedMTLBinaryArchive*> binaryArchives;
  // TODO: when WrappedMTLDynamicLibrary exists
  // rdcarray<WrappedMTLDynamicLibrary*> vertexPreloadedLibraries;
  // rdcarray<WrappedMTLDynamicLibrary*> fragmentPreloadedLibraries;
  LinkedFunctions vertexLinkedFunctions;
  LinkedFunctions fragmentLinkedFunctions;
  bool supportAddingVertexBinaryFunctions = false;
  bool supportAddingFragmentBinaryFunctions = false;
  NS::UInteger maxVertexCallStackDepth = 1;
  NS::UInteger maxFragmentCallStackDepth = 1;
};

// MTLRenderPassAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPass.h
struct RenderPassAttachmentDescriptor
{
  RenderPassAttachmentDescriptor(MTL::LoadAction load, MTL::StoreAction store)
      : loadAction(load), storeAction(store)
  {
  }
  RenderPassAttachmentDescriptor(MTL::RenderPassAttachmentDescriptor *objc);
  void CopyTo(MTL::RenderPassAttachmentDescriptor *objc);
  WrappedMTLTexture *texture = NULL;
  NS::UInteger level = 0;
  NS::UInteger slice = 0;
  NS::UInteger depthPlane = 0;
  WrappedMTLTexture *resolveTexture = NULL;
  NS::UInteger resolveLevel = 0;
  NS::UInteger resolveSlice = 0;
  NS::UInteger resolveDepthPlane = 0;
  MTL::LoadAction loadAction;
  MTL::StoreAction storeAction;
  MTL::StoreActionOptions storeActionOptions = MTL::StoreActionOptionNone;
};

// MTLRenderPassColorAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPass.h
struct RenderPassColorAttachmentDescriptor : RenderPassAttachmentDescriptor
{
  RenderPassColorAttachmentDescriptor()
      : RenderPassAttachmentDescriptor(MTL::LoadActionDontCare, MTL::StoreActionStore)
  {
  }
  RenderPassColorAttachmentDescriptor(MTL::RenderPassColorAttachmentDescriptor *objc);
  void CopyTo(MTL::RenderPassColorAttachmentDescriptor *objc);
  MTL::ClearColor clearColor = MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0);
};

// MTLRenderPassDepthAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPass.h
struct RenderPassDepthAttachmentDescriptor : RenderPassAttachmentDescriptor
{
  RenderPassDepthAttachmentDescriptor()
      : RenderPassAttachmentDescriptor(MTL::LoadActionClear, MTL::StoreActionDontCare)
  {
  }
  RenderPassDepthAttachmentDescriptor(MTL::RenderPassDepthAttachmentDescriptor *objc);
  void CopyTo(MTL::RenderPassDepthAttachmentDescriptor *objc);
  double clearDepth = 1.0;
  MTL::MultisampleDepthResolveFilter depthResolveFilter = MTL::MultisampleDepthResolveFilterSample0;
};

// MTLRenderPassStencilAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPass.h
struct RenderPassStencilAttachmentDescriptor : RenderPassAttachmentDescriptor
{
  RenderPassStencilAttachmentDescriptor()
      : RenderPassAttachmentDescriptor(MTL::LoadActionClear, MTL::StoreActionDontCare)
  {
  }
  RenderPassStencilAttachmentDescriptor(MTL::RenderPassStencilAttachmentDescriptor *objc);
  void CopyTo(MTL::RenderPassStencilAttachmentDescriptor *objc);
  uint32_t clearStencil = 0;
  MTL::MultisampleStencilResolveFilter stencilResolveFilter =
      MTL::MultisampleStencilResolveFilterSample0;
};

// MTLRenderPassSampleBufferAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPass.h
struct RenderPassSampleBufferAttachmentDescriptor
{
  RenderPassSampleBufferAttachmentDescriptor() = default;
  RenderPassSampleBufferAttachmentDescriptor(MTL::RenderPassSampleBufferAttachmentDescriptor *objc);
  void CopyTo(MTL::RenderPassSampleBufferAttachmentDescriptor *objc);
  // TODO: when WrappedMTLCounterSampleBuffer exists
  // WrappedMTLCounterSampleBuffer *sampleBuffer = NULL;
  NS::UInteger startOfVertexSampleIndex = MTLCounterDontSample;
  NS::UInteger endOfVertexSampleIndex = MTLCounterDontSample;
  NS::UInteger startOfFragmentSampleIndex = MTLCounterDontSample;
  NS::UInteger endOfFragmentSampleIndex = MTLCounterDontSample;
};

// MTLRenderPassDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderPass.h
struct RenderPassDescriptor
{
  RenderPassDescriptor() = default;
  RenderPassDescriptor(MTL::RenderPassDescriptor *objc);
  explicit operator MTL::RenderPassDescriptor *();
  rdcarray<RenderPassColorAttachmentDescriptor> colorAttachments;
  RenderPassDepthAttachmentDescriptor depthAttachment;
  RenderPassStencilAttachmentDescriptor stencilAttachment;
  WrappedMTLBuffer *visibilityResultBuffer = NULL;
  NS::UInteger renderTargetArrayLength = 0;
  NS::UInteger imageblockSampleLength = 0;
  NS::UInteger threadgroupMemoryLength = 0;
  NS::UInteger tileWidth = 0;
  NS::UInteger tileHeight = 0;
  NS::UInteger defaultRasterSampleCount = 0;
  NS::UInteger renderTargetWidth = 0;
  NS::UInteger renderTargetHeight = 0;
  rdcarray<MTL::SamplePosition> samplePositions;
  // TODO: when WrappedRasterizationRateMap exists
  // WrappedRasterizationRateMap *rasterizationRateMap = NULL;
  rdcarray<RenderPassSampleBufferAttachmentDescriptor> sampleBufferAttachments;
};

// MTLComputePassSampleBufferAttachmentDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLComputePass.h
struct ComputePassSampleBufferAttachmentDescriptor
{
  ComputePassSampleBufferAttachmentDescriptor() = default;
  ComputePassSampleBufferAttachmentDescriptor(MTL::ComputePassSampleBufferAttachmentDescriptor *objc);
  void CopyTo(MTL::ComputePassSampleBufferAttachmentDescriptor *objc);
  // TODO: when WrappedMTLCounterSampleBuffer exists
  // MTLCounterSampleBuffer *sampleBuffer = NULL;
  NS::UInteger startOfEncoderSampleIndex = MTLCounterDontSample;
  NS::UInteger endOfEncoderSampleIndex = MTLCounterDontSample;
};

// MTLComputePipelineDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLComputePipeline.h
struct ComputePipelineDescriptor
{
  ComputePipelineDescriptor() = default;
  ComputePipelineDescriptor(MTL::ComputePipelineDescriptor *objc);
  explicit operator MTL::ComputePipelineDescriptor *();
  rdcstr label;
  WrappedMTLFunction *computeFunction = NULL;
  bool threadGroupSizeIsMultipleOfThreadExecution = false;
  NS::UInteger maxTotalThreadsPerThreadgroup = 0;
  NS::UInteger maxCallStackDepth = 1;
  StageInputOutputDescriptor stageInputDescriptor;
  rdcarray<RDMTL::PipelineBufferDescriptor> buffers;
  bool supportIndirectCommandBuffers = false;
  // TODO: when WrappedMTLDynamicLibrary exists
  // rdcarray<WrappedMTLDynamicLibrary*> preloadedLibraries;
  // Deprecated
  // rdcarray<WrappedMTLDynamicLibrary*> insertLibraries;
  RDMTL::LinkedFunctions linkedFunctions;
  bool supportAddingBinaryFunctions = false;
  // TODO: when WrappedMTLBinaryArchive exists
  // rdcarray<WrappedMTLBinaryArchive*> binaryArchives;
};

// MTLComputePassDescriptor : based on the interface defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.2.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLComputePass.h
struct ComputePassDescriptor
{
  ComputePassDescriptor() = default;
  ComputePassDescriptor(MTL::ComputePassDescriptor *objc);
  explicit operator MTL::ComputePassDescriptor *();
  rdcarray<ComputePassSampleBufferAttachmentDescriptor> sampleBufferAttachments;
  MTL::DispatchType dispatchType = MTL::DispatchTypeSerial;
};

}    // namespace RDMTL

template <>
inline rdcliteral TypeName<NS::String *>()
{
  return "NSString"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, NS::String *&el);

#define RDMTL_DECLARE_REFLECTION_STRUCT(TYPE)    \
  template <>                                    \
  inline rdcliteral TypeName<RDMTL::TYPE>()      \
  {                                              \
    return STRING_LITERAL(STRINGIZE(MTL##TYPE)); \
  }                                              \
  template <class SerialiserType>                \
  void DoSerialise(SerialiserType &ser, RDMTL::TYPE &el);

RDMTL_DECLARE_REFLECTION_STRUCT(TextureDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPipelineColorAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(PipelineBufferDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(VertexAttributeDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(VertexBufferLayoutDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(VertexDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(AttributeDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(BufferLayoutDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(StageInputOutputDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(FunctionGroup);
RDMTL_DECLARE_REFLECTION_STRUCT(LinkedFunctions);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPipelineDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPassAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPassColorAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPassDepthAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPassStencilAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPassSampleBufferAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(RenderPassDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(ComputePassSampleBufferAttachmentDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(ComputePipelineDescriptor);
RDMTL_DECLARE_REFLECTION_STRUCT(ComputePassDescriptor);
