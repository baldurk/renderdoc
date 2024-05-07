/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "common_pipestate.h"

namespace VKPipe
{
DOCUMENT("A dynamic offset applied to a single descriptor access.");
struct DynamicOffset
{
  DOCUMENT("");
  DynamicOffset() = default;
  DynamicOffset(const DynamicOffset &) = default;
  DynamicOffset &operator=(const DynamicOffset &) = default;

  bool operator==(const DynamicOffset &o) const
  {
    return descriptorByteOffset == o.descriptorByteOffset &&
           dynamicBufferByteOffset == o.dynamicBufferByteOffset;
  }
  bool operator<(const DynamicOffset &o) const
  {
    if(!(descriptorByteOffset == o.descriptorByteOffset))
      return descriptorByteOffset < o.descriptorByteOffset;
    if(!(dynamicBufferByteOffset == o.dynamicBufferByteOffset))
      return dynamicBufferByteOffset < o.dynamicBufferByteOffset;
    return false;
  }
  DOCUMENT(R"(The offset in bytes to the descriptor in the storage.

:type: int
)");
  uint64_t descriptorByteOffset = 0;
  DOCUMENT(R"(The dynamic offset to apply to the buffer in bytes.

:type: int
)");
  uint64_t dynamicBufferByteOffset = 0;
};

DOCUMENT("The contents of a descriptor set.");
struct DescriptorSet
{
  DOCUMENT("");
  DescriptorSet() = default;
  DescriptorSet(const DescriptorSet &) = default;
  DescriptorSet &operator=(const DescriptorSet &) = default;

  bool operator==(const DescriptorSet &o) const
  {
    return layoutResourceId == o.layoutResourceId &&
           descriptorSetResourceId == o.descriptorSetResourceId &&
           pushDescriptor == o.pushDescriptor;
  }
  bool operator<(const DescriptorSet &o) const
  {
    if(!(layoutResourceId == o.layoutResourceId))
      return layoutResourceId < o.layoutResourceId;
    if(!(descriptorSetResourceId == o.descriptorSetResourceId))
      return descriptorSetResourceId < o.descriptorSetResourceId;
    if(!(pushDescriptor == o.pushDescriptor))
      return pushDescriptor < o.pushDescriptor;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the descriptor set layout that matches this set.");
  ResourceId layoutResourceId;
  DOCUMENT("The :class:`ResourceId` of the descriptor set object.");
  ResourceId descriptorSetResourceId;
  DOCUMENT("Indicates if this is a virtual 'push' descriptor set.");
  bool pushDescriptor = false;

  DOCUMENT(R"(A list of dynamic offsets to be applied to specific bindings, on top of the contents
of their descriptors.

.. note::
  The returned values from :meth:`PipeState.GetConstantBuffer` already have these offsets applied.

:type: List[VKDynamicOffset]
)");
  rdcarray<DynamicOffset> dynamicOffsets;
};

DOCUMENT("Describes the object and descriptor set bindings of a Vulkan pipeline object.");
struct Pipeline
{
  DOCUMENT("");
  Pipeline() = default;
  Pipeline(const Pipeline &) = default;
  Pipeline &operator=(const Pipeline &) = default;

  DOCUMENT("The :class:`ResourceId` of the pipeline object.");
  ResourceId pipelineResourceId;
  DOCUMENT("The :class:`ResourceId` of the compute pipeline layout object.");
  ResourceId pipelineComputeLayoutResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the pre-rasterization pipeline layout object.

When not using pipeline libraries, this will be identical to :data:`pipelineFragmentLayoutResourceId`.
)");
  ResourceId pipelinePreRastLayoutResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the fragment pipeline layout object.

When not using pipeline libraries, this will be identical to :data:`pipelinePreRastLayoutResourceId`.
)");
  ResourceId pipelineFragmentLayoutResourceId;
  DOCUMENT("The flags used to create the pipeline object.");
  uint32_t flags = 0;

  DOCUMENT(R"(The bound descriptor sets.

:type: List[VKDescriptorSet]
)");
  rdcarray<DescriptorSet> descriptorSets;
};

DOCUMENT("Describes the Vulkan index buffer binding.")
struct IndexBuffer
{
  DOCUMENT("");
  IndexBuffer() = default;
  IndexBuffer(const IndexBuffer &) = default;
  IndexBuffer &operator=(const IndexBuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The number of bytes for each index in the index buffer. Typically 2 or 4 bytes but
it can be 0 if no index buffer is bound.
)");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the vulkan input assembly configuration.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;
  InputAssembly &operator=(const InputAssembly &) = default;

  DOCUMENT("``True`` if primitive restart is enabled for strip primitives.");
  bool primitiveRestartEnable = false;

  DOCUMENT(R"(The index buffer binding.

:type: VKIndexBuffer
)");
  IndexBuffer indexBuffer;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;
};

DOCUMENT("Describes the configuration of a single vertex attribute.");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;
  VertexAttribute &operator=(const VertexAttribute &) = default;

  bool operator==(const VertexAttribute &o) const
  {
    return location == o.location && binding == o.binding && format == o.format &&
           byteOffset == o.byteOffset;
  }
  bool operator<(const VertexAttribute &o) const
  {
    if(!(location == o.location))
      return location < o.location;
    if(!(binding == o.binding))
      return binding < o.binding;
    if(!(format == o.format))
      return format < o.format;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The location in the shader that is bound to this attribute.");
  uint32_t location = 0;
  DOCUMENT("The vertex binding where data will be sourced from.");
  uint32_t binding = 0;
  DOCUMENT(R"(The format describing how the input element is interpreted.

:type: ResourceFormat
)");
  ResourceFormat format;
  DOCUMENT(
      "The byte offset from the start of each vertex data in the :data:`binding` to this "
      "attribute.");
  uint32_t byteOffset = 0;
};

DOCUMENT("Describes a vertex binding.");
struct VertexBinding
{
  DOCUMENT("");
  VertexBinding() = default;
  VertexBinding(const VertexBinding &) = default;
  VertexBinding &operator=(const VertexBinding &) = default;

  bool operator==(const VertexBinding &o) const
  {
    return vertexBufferBinding == o.vertexBufferBinding && perInstance == o.perInstance &&
           instanceDivisor == o.instanceDivisor;
  }
  bool operator<(const VertexBinding &o) const
  {
    if(!(vertexBufferBinding == o.vertexBufferBinding))
      return vertexBufferBinding < o.vertexBufferBinding;
    if(!(perInstance == o.perInstance))
      return perInstance < o.perInstance;
    if(!(instanceDivisor == o.instanceDivisor))
      return instanceDivisor < o.instanceDivisor;
    return false;
  }
  DOCUMENT("The vertex binding where data will be sourced from.");
  uint32_t vertexBufferBinding = 0;
  DOCUMENT("``True`` if the vertex data is instance-rate.");
  bool perInstance = false;
  DOCUMENT(R"(The instance rate divisor.

If this is ``0`` then every vertex gets the same value.

If it's ``1`` then one element is read for each instance, and for ``N`` greater than ``1`` then
``N`` instances read the same element before advancing.
)");
  uint32_t instanceDivisor = 1;
};

DOCUMENT("Describes a single Vulkan vertex buffer binding.")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteStride == o.byteStride &&
           byteSize == o.byteSize;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;
  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint64_t byteOffset = 0;
  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
  DOCUMENT("The size of the vertex buffer.");
  uint32_t byteSize = 0;
};

DOCUMENT("Describes the fixed-function vertex input fetch setup.");
struct VertexInput
{
  DOCUMENT("");
  VertexInput() = default;
  VertexInput(const VertexInput &) = default;
  VertexInput &operator=(const VertexInput &) = default;

  DOCUMENT(R"(The vertex attributes.

:type: List[VKVertexAttribute]
)");
  rdcarray<VertexAttribute> attributes;
  DOCUMENT(R"(The vertex bindings.

:type: List[VKVertexBinding]
)");
  rdcarray<VertexBinding> bindings;
  DOCUMENT(R"(The vertex buffers.

:type: List[VKVertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;
};

DOCUMENT("Describes a Vulkan shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader module object.");
  ResourceId resourceId;
  DOCUMENT("The name of the entry point in the shader module that is used.");
  rdcstr entryPoint;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  ShaderReflection *reflection = NULL;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT("The byte offset into the push constant data that is visible to this shader.");
  uint32_t pushConstantRangeByteOffset = 0;

  DOCUMENT("The number of bytes in the push constant data that is visible to this shader.");
  uint32_t pushConstantRangeByteSize = 0;

  DOCUMENT("The required subgroup size specified for this shader at pipeline creation time.");
  uint32_t requiredSubgroupSize = 0;

  DOCUMENT(R"(The provided specialization constant data. Shader constants store the byte offset into
this buffer as their byteOffset. This data includes the applied specialization constants over the
top of the default values, so it is safe to read any constant from here and get the correct current
value.

:type: bytes
)");
  bytebuf specializationData;

  DOCUMENT(R"(The specialization constant ID for each entry in the specialization constant block of
reflection info. This corresponds to the constantID in VkSpecializationMapEntry, while the offset
and size into specializationData can be obtained from the reflection info.

:type: List[int]
)")
  rdcarray<uint32_t> specializationIds;

  DOCUMENT("Whether the shader is a shader object or shader module.");
  bool shaderObject = false;
};

DOCUMENT("Describes the state of the fixed-function tessellator.");
struct Tessellation
{
  DOCUMENT("");
  Tessellation() = default;
  Tessellation(const Tessellation &) = default;
  Tessellation &operator=(const Tessellation &) = default;

  DOCUMENT("The number of control points in each input patch.");
  uint32_t numControlPoints = 0;

  DOCUMENT("``True`` if the tessellation domain origin is upper-left, ``False`` if lower-left.");
  bool domainOriginUpperLeft = true;
};

DOCUMENT("Describes a single transform feedback binding.");
struct XFBBuffer
{
  DOCUMENT("");
  XFBBuffer() = default;
  XFBBuffer(const XFBBuffer &) = default;
  XFBBuffer &operator=(const XFBBuffer &) = default;

  bool operator==(const XFBBuffer &o) const
  {
    return active == o.active && bufferResourceId == o.bufferResourceId &&
           byteOffset == o.byteOffset && byteSize == o.byteSize &&
           counterBufferResourceId == o.counterBufferResourceId &&
           counterBufferOffset == o.counterBufferOffset;
  }
  bool operator<(const XFBBuffer &o) const
  {
    if(!(active == o.active))
      return active < o.active;
    if(!(bufferResourceId == o.bufferResourceId))
      return bufferResourceId < o.bufferResourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(counterBufferResourceId == o.counterBufferResourceId))
      return counterBufferResourceId < o.counterBufferResourceId;
    if(!(counterBufferOffset == o.counterBufferOffset))
      return counterBufferOffset < o.counterBufferOffset;
    return false;
  }

  DOCUMENT("A flag indicating if this buffer is active or not.");
  bool active = false;

  DOCUMENT("The :class:`ResourceId` of the bound data buffer.");
  ResourceId bufferResourceId;

  DOCUMENT("The offset in bytes to the start of the data in the :data:`bufferResourceId`.");
  uint64_t byteOffset = 0;

  DOCUMENT("The size in bytes of the data buffer.");
  uint64_t byteSize = 0;

  DOCUMENT("The :class:`ResourceId` of the buffer storing the counter value (if set).");
  ResourceId counterBufferResourceId;

  DOCUMENT("The offset in bytes to the counter in the :data:`counterBufferResourceId`.");
  uint64_t counterBufferOffset = 0;
};

DOCUMENT("Describes the state of the fixed-function transform feedback.");
struct TransformFeedback
{
  DOCUMENT("");
  TransformFeedback() = default;
  TransformFeedback(const TransformFeedback &) = default;
  TransformFeedback &operator=(const TransformFeedback &) = default;

  DOCUMENT(R"(The bound transform feedback buffers.

:type: List[VKXFBBuffer]
)");
  rdcarray<XFBBuffer> buffers;

  DOCUMENT(R"(Which stream-out stream is being used for rasterization.

:type: int
)");
  uint32_t rasterizedStream = 0;
};

DOCUMENT("Describes a render area in the current framebuffer.");
struct RenderArea
{
  DOCUMENT("");
  RenderArea() = default;
  RenderArea(const RenderArea &) = default;
  RenderArea &operator=(const RenderArea &) = default;
  bool operator==(const RenderArea &o) const
  {
    return x == o.x && y == o.y && width == o.width && height == o.height;
  }
  bool operator<(const RenderArea &o) const
  {
    if(!(x == o.x))
      return x < o.x;
    if(!(y == o.y))
      return y < o.y;
    if(!(width == o.width))
      return width < o.width;
    if(!(height == o.height))
      return height < o.height;
    return false;
  }

  DOCUMENT("The X co-ordinate of the render area.");
  int32_t x = 0;
  DOCUMENT("The Y co-ordinate of the render area.");
  int32_t y = 0;
  DOCUMENT("The width of the render area.");
  int32_t width = 0;
  DOCUMENT("The height of the render area.");
  int32_t height = 0;
};

DOCUMENT("Describes a combined viewport and scissor region.");
struct ViewportScissor
{
  DOCUMENT("");
  ViewportScissor() = default;
  ViewportScissor(const ViewportScissor &) = default;
  ViewportScissor &operator=(const ViewportScissor &) = default;

  bool operator==(const ViewportScissor &o) const { return vp == o.vp && scissor == o.scissor; }
  bool operator<(const ViewportScissor &o) const
  {
    if(!(vp == o.vp))
      return vp < o.vp;
    if(!(scissor == o.scissor))
      return scissor < o.scissor;
    return false;
  }
  DOCUMENT(R"(The viewport.

:type: Viewport
)");
  Viewport vp;
  DOCUMENT(R"(The scissor.

:type: Scissor
)");
  Scissor scissor;
};

DOCUMENT("Describes the view state in the pipeline.");
struct ViewState
{
  DOCUMENT("");
  ViewState() = default;
  ViewState(const ViewState &) = default;
  ViewState &operator=(const ViewState &) = default;

  DOCUMENT(R"(The bound viewports and scissors.

:type: List[VKViewportScissor]
)");
  rdcarray<ViewportScissor> viewportScissors;

  DOCUMENT(R"(The discard rectangles, if enabled.

:type: List[VKRenderArea]
)");
  rdcarray<RenderArea> discardRectangles;

  DOCUMENT(R"(``True`` if a fragment in any one of the discard rectangles fails the discard test,
and a fragment in none of them passes.

``False`` if a fragment in any one of the discard rectangles passes the discard test,
and a fragment in none of them is discarded.

.. note::
  A ``True`` value and an empty list of :data:`discardRectangles` means the test is effectively
  disabled, since with no rectangles no fragment can be inside one.
)");
  bool discardRectanglesExclusive = true;

  DOCUMENT(R"(Whether depth clip range is set to [-1, 1] through VK_EXT_depth_clip_control.)");
  bool depthNegativeOneToOne = false;
};

DOCUMENT("Describes the rasterizer state in the pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clamped and
to ``0.0`` to ``1.0``.
)");
  bool depthClampEnable = false;
  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clipped.

.. note::
  In Vulkan 1.0 this value was implicitly set to the opposite of :data:`depthClampEnable`, but with
  later extensions & versions it can be set independently.
)");
  bool depthClipEnable = true;
  DOCUMENT("``True`` if primitives should be discarded during rasterization.");
  bool rasterizerDiscardEnable = false;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool frontCCW = false;
  DOCUMENT("The polygon :class:`FillMode`.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon :class:`CullMode`.");
  CullMode cullMode = CullMode::NoCull;

  DOCUMENT("The active conservative rasterization mode.");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;

  DOCUMENT(R"(The extra size in pixels to increase primitives by during conservative rasterization,
in the x and y directions in screen space.

See :data:`conservativeRasterizationMode`
)");
  float extraPrimitiveOverestimationSize = 0.0f;

  DOCUMENT("Whether the provoking vertex is the first one (default behaviour).");
  bool provokingVertexFirst = true;
  DOCUMENT("Whether depth biasing is enabled.");
  bool depthBiasEnable = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  float depthBias = 0.0f;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float depthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT("The fixed line width in pixels.");
  float lineWidth = 0.0f;

  DOCUMENT("The line rasterization mode.");
  LineRaster lineRasterMode = LineRaster::Default;
  DOCUMENT("The line stipple factor, or 0 if line stipple is disabled.");
  uint32_t lineStippleFactor = 0;
  DOCUMENT("The line stipple bit-pattern.");
  uint16_t lineStipplePattern = 0;
  DOCUMENT(R"(The current pipeline fragment shading rate. This will always be 1x1 when a fragment
shading rate has not been specified.

:type: Tuple[int,int]
)");
  rdcpair<uint32_t, uint32_t> pipelineShadingRate = {1, 1};
  DOCUMENT(R"(The fragment shading rate combiners.

The combiners are applied as follows, according to the Vulkan spec:

  ``intermediateRate = combiner[0] ( pipelineShadingRate,  shaderExportedShadingRate )``
  ``finalRate        = combiner[1] ( intermediateRate,     imageBasedShadingRate     )``

Where the first input is from :data:`pipelineShadingRate` and the second is the exported shading
rate from the last pre-rasterization shader stage, which defaults to 1x1 if not exported.

The intermediate result is then used as the first input to the second combiner, together with the
shading rate sampled from the fragment shading rate attachment.

:type: Tuple[ShadingRateCombiner,ShadingRateCombiner]
)");
  rdcpair<ShadingRateCombiner, ShadingRateCombiner> shadingRateCombiners = {
      ShadingRateCombiner::Keep, ShadingRateCombiner::Keep};
};

DOCUMENT("Describes state of custom sample locations in the pipeline.");
struct SampleLocations
{
  DOCUMENT("");
  SampleLocations() = default;
  SampleLocations(const SampleLocations &) = default;
  SampleLocations &operator=(const SampleLocations &) = default;

  DOCUMENT("The width in pixels of the region configured.");
  uint32_t gridWidth = 1;
  DOCUMENT("The height in pixels of the region configured.");
  uint32_t gridHeight = 1;
  DOCUMENT(R"(The custom sample locations. Only x and y are valid, z and w are set to 0.0.

If the list is empty then the standard sample pattern is in use.

:type: List[FloatVector]
)");
  rdcarray<FloatVector> customLocations;
};

DOCUMENT("Describes the multisampling state in the pipeline.");
struct MultiSample
{
  DOCUMENT("");
  MultiSample() = default;
  MultiSample(const MultiSample &) = default;
  MultiSample &operator=(const MultiSample &) = default;

  DOCUMENT("How many samples to use when rasterizing.");
  uint32_t rasterSamples = 0;
  DOCUMENT("``True`` if rendering should happen at sample-rate frequency.");
  bool sampleShadingEnable = false;
  DOCUMENT("The minimum sample shading rate.");
  float minSampleShading = 0.0f;
  DOCUMENT("A mask that generated samples should be masked with using bitwise ``AND``.");
  uint32_t sampleMask = 0;
  DOCUMENT(R"(The custom sample locations configuration.

:type: VKSampleLocations
)");
  SampleLocations sampleLocations;
};

DOCUMENT("Describes the pipeline blending state.");
struct ColorBlendState
{
  DOCUMENT("");
  ColorBlendState() = default;
  ColorBlendState(const ColorBlendState &) = default;
  ColorBlendState &operator=(const ColorBlendState &) = default;

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverageEnable = false;
  DOCUMENT("``True`` if alpha-to-one should be used when blending to an MSAA target.");
  bool alphaToOneEnable = false;

  DOCUMENT(R"(The blend operations for each target.

:type: List[ColorBlend]
)");
  rdcarray<ColorBlend> blends;

  DOCUMENT(R"(The constant blend factor to use in blend equations.

:type: Tuple[float,float,float,float]
)");
  rdcfixedarray<float, 4> blendFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the pipeline depth-stencil state.");
struct DepthStencil
{
  DOCUMENT("");
  DepthStencil() = default;
  DepthStencil(const DepthStencil &) = default;
  DepthStencil &operator=(const DepthStencil &) = default;

  DOCUMENT("``True`` if depth testing should be performed.");
  bool depthTestEnable = false;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool depthWriteEnable = false;
  DOCUMENT("``True`` if depth bounds tests should be applied.");
  bool depthBoundsEnable = false;
  DOCUMENT("The :class:`CompareFunction` to use for testing depth values.");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;

  DOCUMENT("``True`` if stencil operations should be performed.");
  bool stencilTestEnable = false;

  DOCUMENT(R"(The stencil state for front-facing polygons.

:type: StencilFace
)");
  StencilFace frontFace;

  DOCUMENT(R"(The stencil state for back-facing polygons.

:type: StencilFace
)");
  StencilFace backFace;

  DOCUMENT("The near plane bounding value.");
  float minDepthBounds = 0.0f;
  DOCUMENT("The far plane bounding value.");
  float maxDepthBounds = 0.0f;
};

DOCUMENT("Describes the setup of a renderpass and subpasses.");
struct RenderPass
{
  DOCUMENT("");
  RenderPass() = default;
  RenderPass(const RenderPass &) = default;
  RenderPass &operator=(const RenderPass &) = default;

  DOCUMENT("The :class:`ResourceId` of the render pass.");
  ResourceId resourceId;

  DOCUMENT("Whether or not dynamic rendering is in use (no render pass or framebuffer objects).");
  bool dynamic = false;

  DOCUMENT("Whether or not dynamic rendering is currently suspended.");
  bool suspended = false;

  DOCUMENT("Whether or not there is a potential feedback loop.");
  bool feedbackLoop = false;

  DOCUMENT("The index of the current active subpass.");
  uint32_t subpass;

  // VKTODOMED renderpass and subpass information here

  DOCUMENT(R"(The input attachments for the current subpass, as indices into the framebuffer
attachments.

:type: List[int]
)");
  rdcarray<uint32_t> inputAttachments;
  DOCUMENT(R"(The color attachments for the current subpass, as indices into the framebuffer
attachments.

:type: List[int]
)");
  rdcarray<uint32_t> colorAttachments;
  DOCUMENT(R"(The resolve attachments for the current subpass, as indices into the framebuffer
attachments.

:type: List[int]
)");
  rdcarray<uint32_t> resolveAttachments;
  DOCUMENT(R"(An index into the framebuffer attachments for the depth-stencil attachment.

If there is no depth-stencil attachment, this index is ``-1``.
)");
  int32_t depthstencilAttachment = -1;
  DOCUMENT(R"(An index into the framebuffer attachments for the depth-stencil resolve attachment.

If there is no depth-stencil resolve attachment, this index is ``-1``.
)");
  int32_t depthstencilResolveAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment density attachment.

If there is no fragment density attachment, this index is ``-1``.

.. note::
  Only one at most of :data:`fragmentDensityAttachment` and :data:`shadingRateAttachment` will be
  set.
)");
  int32_t fragmentDensityAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment shading rate attachment.

If there is no fragment shading rate attachment, this index is ``-1``.

.. note::
  Only one at most of :data:`fragmentDensityAttachment` and :data:`shadingRateAttachment` will be
  set.
)");
  int32_t shadingRateAttachment = -1;

  DOCUMENT(R"(The size of the framebuffer region represented by each texel in
:data:`shadingRateAttachment`.

For example if this is (2,2) then every texel in the attachment gives the shading rate of a 2x2
block in the framebuffer so the shading rate attachment is half the size of the other attachments in
each dimension.

If no attachment is set in :data:`shadingRateAttachment` this will be (1,1).

:type: Tuple[int,int]
)");
  rdcpair<uint32_t, uint32_t> shadingRateTexelSize = {1, 1};

  DOCUMENT(R"(If multiview is enabled, contains a list of view indices to be broadcast to during
rendering.

If the list is empty, multiview is disabled and rendering is as normal.

:type: List[int]
)");
  rdcarray<uint32_t> multiviews;

  DOCUMENT(R"(If VK_QCOM_fragment_density_map_offset is enabled, contains a list of offsets applied 
to the fragment density map during rendering.

If the list is empty, fdm_offset is disabled and rendering is as normal.

:type: List[Offset]
)");
  rdcarray<Offset> fragmentDensityOffsets;

  DOCUMENT(R"(If VK_EXT_multisampled_render_to_single_sampled is enabled, contains the number of
samples used to render this subpass.

If the subpass is not internally multisampled, tileOnlyMSAASampleCount is set to 0.
)");
  uint32_t tileOnlyMSAASampleCount = 0;
};

DOCUMENT("Describes a framebuffer object and its attachments.");
struct Framebuffer
{
  DOCUMENT("");
  Framebuffer() = default;
  Framebuffer(const Framebuffer &) = default;
  Framebuffer &operator=(const Framebuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the framebuffer object.");
  ResourceId resourceId;

  DOCUMENT(R"(The attachments of this framebuffer.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> attachments;

  DOCUMENT("The width of this framebuffer in pixels.");
  uint32_t width = 0;
  DOCUMENT("The height of this framebuffer in pixels.");
  uint32_t height = 0;
  DOCUMENT("The number of layers in this framebuffer.");
  uint32_t layers = 0;
};

DOCUMENT("Describes the current pass instance at the current time.");
struct CurrentPass
{
  DOCUMENT("");
  CurrentPass() = default;
  CurrentPass(const CurrentPass &) = default;
  CurrentPass &operator=(const CurrentPass &) = default;

  DOCUMENT(R"(The renderpass and subpass that is currently active.

:type: VKRenderPass
)");
  RenderPass renderpass;
  DOCUMENT(R"(The framebuffer that is currently being used.

:type: VKFramebuffer
)");
  Framebuffer framebuffer;
  DOCUMENT(R"(The render area that is currently being rendered to.

:type: VKRenderArea
)");
  RenderArea renderArea;

  DOCUMENT("If feedback loops are allowed on color attachments");
  bool colorFeedbackAllowed = false;

  DOCUMENT("If feedback loops are allowed on depth attachments");
  bool depthFeedbackAllowed = false;

  DOCUMENT("If feedback loops are allowed on stencil attachments");
  bool stencilFeedbackAllowed = false;
};

DOCUMENT("Contains the layout of a range of subresources in an image.");
struct ImageLayout
{
  DOCUMENT("");
  ImageLayout() = default;
  ImageLayout(const ImageLayout &) = default;
  ImageLayout &operator=(const ImageLayout &) = default;

  bool operator==(const ImageLayout &o) const
  {
    return baseMip == o.baseMip && baseLayer == o.baseLayer && numMip == o.numMip &&
           numLayer == o.numLayer && name == o.name;
  }
  bool operator<(const ImageLayout &o) const
  {
    if(!(baseMip == o.baseMip))
      return baseMip < o.baseMip;
    if(!(baseLayer == o.baseLayer))
      return baseLayer < o.baseLayer;
    if(!(numMip == o.numMip))
      return numMip < o.numMip;
    if(!(numLayer == o.numLayer))
      return numLayer < o.numLayer;
    if(!(name == o.name))
      return name < o.name;
    return false;
  }
  DOCUMENT("The first mip level used in the range.");
  uint32_t baseMip = 0;
  DOCUMENT("For 3D textures and texture arrays, the first slice used in the range.");
  uint32_t baseLayer = 0;
  DOCUMENT("The number of mip levels in the range.");
  uint32_t numMip = 1;
  DOCUMENT("For 3D textures and texture arrays, the number of array slices in the range.");
  uint32_t numLayer = 1;
  DOCUMENT("The name of the current image state.");
  rdcstr name;
};

DOCUMENT("Contains the current layout of all subresources in the image.");
struct ImageData
{
  DOCUMENT("");
  ImageData() = default;
  ImageData(const ImageData &) = default;
  ImageData &operator=(const ImageData &) = default;

  bool operator==(const ImageData &o) const { return resourceId == o.resourceId; }
  bool operator<(const ImageData &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the image.");
  ResourceId resourceId;

  DOCUMENT(R"(The subresource regions in this resource.

:type: List[VKImageLayout]
)");
  rdcarray<ImageLayout> layouts;
};

DOCUMENT("Contains the current conditional rendering state.");
struct ConditionalRendering
{
  DOCUMENT("");
  ConditionalRendering() = default;
  ConditionalRendering(const ConditionalRendering &) = default;
  ConditionalRendering &operator=(const ConditionalRendering &) = default;

  DOCUMENT(
      "The :class:`ResourceId` of the buffer containing the predicate for conditional rendering.");
  ResourceId bufferId;

  DOCUMENT("The byte offset into buffer where the predicate is located.");
  uint64_t byteOffset = 0;

  DOCUMENT("``True`` if predicate result is inverted.");
  bool isInverted = false;

  DOCUMENT("``True`` if the current predicate would render.");
  bool isPassing = false;
};

DOCUMENT("The full current Vulkan pipeline state.");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT(R"(The currently bound compute pipeline, if any.

:type: VKPipeline
)");
  Pipeline compute;
  DOCUMENT(R"(The currently bound graphics pipeline, if any.

:type: VKPipeline
)");
  Pipeline graphics;

  DOCUMENT(R"(The raw push constant data.

:type: bytes
)");
  bytebuf pushconsts;

  DOCUMENT(R"(The input assembly stage.

:type: VKInputAssembly
)");
  InputAssembly inputAssembly;
  DOCUMENT(R"(The vertex input stage.

:type: VKVertexInput
)");
  VertexInput vertexInput;

  DOCUMENT(R"(The vertex shader stage.

:type: VKShader
)");
  Shader vertexShader;
  DOCUMENT(R"(The tessellation control shader stage.

:type: VKShader
)");
  Shader tessControlShader;
  DOCUMENT(R"(The tessellation evaluation shader stage.

:type: VKShader
)");
  Shader tessEvalShader;
  DOCUMENT(R"(The geometry shader stage.

:type: VKShader
)");
  Shader geometryShader;
  DOCUMENT(R"(The fragment shader stage.

:type: VKShader
)");
  Shader fragmentShader;
  DOCUMENT(R"(The compute shader stage.

:type: VKShader
)");
  Shader computeShader;

  DOCUMENT(R"(The task shader stage.

:type: VKShader
)");
  Shader taskShader;

  DOCUMENT(R"(The mesh shader stage.

:type: VKShader
)");
  Shader meshShader;

  DOCUMENT(R"(The tessellation stage.

:type: VKTessellation
)");
  Tessellation tessellation;

  DOCUMENT(R"(The transform feedback stage.

:type: VKTransformFeedback
)");
  TransformFeedback transformFeedback;

  DOCUMENT(R"(The viewport setup.

:type: VKViewState
)");
  ViewState viewportScissor;
  DOCUMENT(R"(The rasterization configuration.

:type: VKRasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The multisampling configuration.

:type: VKMultiSample
)");
  MultiSample multisample;
  DOCUMENT(R"(The color blending configuration.

:type: VKColorBlendState
)");
  ColorBlendState colorBlend;
  DOCUMENT(R"(The depth-stencil state.

:type: VKDepthStencil
)");
  DepthStencil depthStencil;

  DOCUMENT(R"(The current renderpass, subpass and framebuffer.

:type: VKCurrentPass
)");
  CurrentPass currentPass;

  DOCUMENT(R"(The resource states for the currently live resources.

:type: List[VKImageData]
)");
  rdcarray<ImageData> images;

  DOCUMENT(R"(The shader messages retrieved for this action.

:type: List[ShaderMessage]
)");
  rdcarray<ShaderMessage> shaderMessages;

  DOCUMENT(R"(The current conditional rendering state.

:type: VKConditionalRendering
)");
  ConditionalRendering conditionalRendering;
};

};    // namespace VKPipe

DECLARE_REFLECTION_STRUCT(VKPipe::DynamicOffset);
DECLARE_REFLECTION_STRUCT(VKPipe::DescriptorSet);
DECLARE_REFLECTION_STRUCT(VKPipe::Pipeline);
DECLARE_REFLECTION_STRUCT(VKPipe::IndexBuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::InputAssembly);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexBinding);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexInput);
DECLARE_REFLECTION_STRUCT(VKPipe::Shader);
DECLARE_REFLECTION_STRUCT(VKPipe::Tessellation);
DECLARE_REFLECTION_STRUCT(VKPipe::XFBBuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::TransformFeedback);
DECLARE_REFLECTION_STRUCT(VKPipe::ViewportScissor);
DECLARE_REFLECTION_STRUCT(VKPipe::ViewState);
DECLARE_REFLECTION_STRUCT(VKPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(VKPipe::SampleLocations);
DECLARE_REFLECTION_STRUCT(VKPipe::MultiSample);
DECLARE_REFLECTION_STRUCT(VKPipe::ColorBlendState);
DECLARE_REFLECTION_STRUCT(VKPipe::DepthStencil);
DECLARE_REFLECTION_STRUCT(VKPipe::RenderPass);
DECLARE_REFLECTION_STRUCT(VKPipe::Framebuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::RenderArea);
DECLARE_REFLECTION_STRUCT(VKPipe::CurrentPass);
DECLARE_REFLECTION_STRUCT(VKPipe::ImageLayout);
DECLARE_REFLECTION_STRUCT(VKPipe::ImageData);
DECLARE_REFLECTION_STRUCT(VKPipe::ConditionalRendering);
DECLARE_REFLECTION_STRUCT(VKPipe::State);
