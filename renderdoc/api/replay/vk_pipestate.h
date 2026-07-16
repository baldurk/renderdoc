/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
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

  DOCUMENT(R"(The :class:`ResourceId` of the descriptor set layout that matches this set.

:type: ResourceId
)");
  ResourceId layoutResourceId;

  DOCUMENT(R"(The :class:`ResourceId` of the descriptor set object, if a real descriptor set is bound.

.. note::
  If using descriptor buffers this value may be unset, see :data:`descriptorBufferIndex`.

:type: ResourceId
)");
  ResourceId descriptorSetResourceId;

  DOCUMENT(R"(Indicates if this is a virtual 'push' descriptor set.

:type: bool
)");
  bool pushDescriptor = false;

  DOCUMENT(R"(A list of dynamic offsets to be applied to specific bindings, on top of the contents
of their descriptors.

.. note::
  The returned values from :meth:`PipeState.GetConstantBlock` already have these offsets applied.

:type: List[VKDynamicOffset]
)");
  rdcarray<DynamicOffset> dynamicOffsets;

  DOCUMENT(R"(The index of the descriptor buffer to be used, or ``-1`` if no descriptor buffer is used.

:type: int
)");
  int32_t descriptorBufferIndex = -1;

  DOCUMENT(R"(The byte offset from the start of the descriptor buffer at index :data:`descriptorBufferIndex` where this set's data is.

:type: int
)");
  uint64_t descriptorBufferByteOffset = 0;

  DOCUMENT(R"(Indicates if this is a virtual descriptor set for binding embedded immutable samplers.

:type: bool
)");
  bool descriptorBufferEmbeddedSamplers = false;
};

DOCUMENT("A single descriptor buffer binding.");
struct DescriptorBuffer
{
  DOCUMENT("");
  DescriptorBuffer() = default;
  DescriptorBuffer(const DescriptorBuffer &) = default;
  DescriptorBuffer &operator=(const DescriptorBuffer &) = default;

  bool operator==(const DescriptorBuffer &o) const
  {
    return buffer == o.buffer && offset == o.offset && pushDescriptor == o.pushDescriptor &&
           pushBuffer == o.pushBuffer && resourceBuffer == o.resourceBuffer &&
           samplerBuffer == o.samplerBuffer;
  }
  bool operator<(const DescriptorBuffer &o) const
  {
    if(!(buffer == o.buffer))
      return buffer < o.buffer;
    if(!(offset == o.offset))
      return offset < o.offset;
    if(!(pushDescriptor == o.pushDescriptor))
      return pushDescriptor < o.pushDescriptor;
    if(!(pushBuffer == o.pushBuffer))
      return pushBuffer < o.pushBuffer;
    if(!(resourceBuffer == o.resourceBuffer))
      return resourceBuffer < o.resourceBuffer;
    if(!(samplerBuffer == o.samplerBuffer))
      return samplerBuffer < o.samplerBuffer;
    return false;
  }

  DOCUMENT(R"(The :class:`ResourceId` of the buffer object being bound.

:type: ResourceId
)");
  ResourceId buffer;

  DOCUMENT(R"(The offset in bytes from the base of the buffer object to the start of the bound region.

:type: int
)");
  uint64_t offset = 0;

  DOCUMENT(R"(Indicates if this is the 'push' descriptor buffer.

:type: bool
)");
  bool pushDescriptor = false;

  DOCUMENT(R"(For push descriptors where a buffer is required, the :class:`ResourceId` of the push buffer object.

:type: ResourceId
)");
  ResourceId pushBuffer;

  DOCUMENT(R"(Indicates if this buffer can contain resources (buffers and images).

:type: bool
)");
  bool resourceBuffer = false;

  DOCUMENT(R"(Indicates if this buffer can contain samplers.

:type: bool
)");
  bool samplerBuffer = false;
};

DOCUMENT("Describes the object and descriptor set bindings of a Vulkan pipeline object.");
struct Pipeline
{
  DOCUMENT("");
  Pipeline() = default;
  Pipeline(const Pipeline &) = default;
  Pipeline &operator=(const Pipeline &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the pipeline object.

:type: ResourceId
)");
  ResourceId pipelineResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the compute pipeline layout object.

:type: ResourceId
)");
  ResourceId pipelineComputeLayoutResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the pre-rasterization pipeline layout object.

When not using pipeline libraries, this will be identical to :data:`pipelineFragmentLayoutResourceId`.

:type: ResourceId
)");
  ResourceId pipelinePreRastLayoutResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the fragment pipeline layout object.

When not using pipeline libraries, this will be identical to :data:`pipelinePreRastLayoutResourceId`.

:type: ResourceId
)");
  ResourceId pipelineFragmentLayoutResourceId;
  DOCUMENT(R"(The flags used to create the pipeline object.

:type: int
)");
  uint64_t flags = 0;

  DOCUMENT(R"(The bound descriptor sets.

:type: List[VKDescriptorSet]
)");
  rdcarray<DescriptorSet> descriptorSets;

  DOCUMENT(R"(The bound descriptor buffers.

:type: List[VKDescriptorBuffer]
)");
  rdcarray<DescriptorBuffer> descriptorBuffers;
};

DOCUMENT("Describes the Vulkan index buffer binding.")
struct IndexBuffer
{
  DOCUMENT("");
  IndexBuffer() = default;
  IndexBuffer(const IndexBuffer &) = default;
  IndexBuffer &operator=(const IndexBuffer &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the index buffer.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The byte offset from the start of the buffer to the beginning of the index data.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The byte size from the start offset to the end of the index data.

:type: int
)");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The number of bytes for each index in the index buffer. Typically 2 or 4 bytes but
it can be 0 if no index buffer is bound.

:type: int
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

  DOCUMENT(R"(``True`` if primitive restart is enabled for strip primitives.

:type: bool
)");
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
  DOCUMENT(R"(The location in the shader that is bound to this attribute.

:type: int
)");
  uint32_t location = 0;
  DOCUMENT(R"(The vertex binding where data will be sourced from.

:type: int
)");
  uint32_t binding = 0;
  DOCUMENT(R"(The format describing how the input element is interpreted.

:type: ResourceFormat
)");
  ResourceFormat format;
  DOCUMENT(R"(The byte offset from the start of each vertex data in the :data:`binding` to this attribute.

:type: int
)");
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
  DOCUMENT(R"(The vertex binding where data will be sourced from.

:type: int
)");
  uint32_t vertexBufferBinding = 0;
  DOCUMENT(R"(``True`` if the vertex data is instance-rate.

:type: bool
)");
  bool perInstance = false;
  DOCUMENT(R"(The instance rate divisor.

If this is ``0`` then every vertex gets the same value.

If it's ``1`` then one element is read for each instance, and for ``N`` greater than ``1`` then
``N`` instances read the same element before advancing.

:type: int
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
  DOCUMENT(R"(The :class:`ResourceId` of the buffer bound to this slot.

:type: ResourceId
)");
  ResourceId resourceId;
  DOCUMENT(R"(The byte offset from the start of the buffer to the beginning of the vertex data.

:type: int
)");
  uint64_t byteOffset = 0;
  DOCUMENT(R"(The byte stride between the start of one set of vertex data and the next.

:type: int
)");
  uint32_t byteStride = 0;
  DOCUMENT(R"(The size of the vertex buffer.

:type: int
)");
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

  DOCUMENT(R"(The :class:`ResourceId` of the shader module object.

:type: ResourceId
)");
  ResourceId resourceId;
  DOCUMENT(R"(The name of the entry point in the shader module that is used.

:type: str)");
  rdcstr entryPoint;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  const ShaderReflection *reflection = NULL;

  DOCUMENT(R"(A :class:`ShaderStage` identifying which stage this shader is bound to.

:type: ShaderStage
)");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT(R"(The byte offset into the push constant data that is visible to this shader.

:type: int
)");
  uint32_t pushConstantRangeByteOffset = 0;

  DOCUMENT(R"(The number of bytes in the push constant data that is visible to this shader.

:type: int
)");
  uint32_t pushConstantRangeByteSize = 0;

  DOCUMENT(R"(The required subgroup size specified for this shader at pipeline creation time.

:type: int
)");
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

  DOCUMENT(R"(Whether the shader is a shader object or shader module.

:type: bool
)");
  bool shaderObject = false;
};

DOCUMENT("Describes the state of the fixed-function tessellator.");
struct Tessellation
{
  DOCUMENT("");
  Tessellation() = default;
  Tessellation(const Tessellation &) = default;
  Tessellation &operator=(const Tessellation &) = default;

  DOCUMENT(R"(The number of control points in each input patch.

:type: int
)");
  uint32_t numControlPoints = 0;

  DOCUMENT(R"(``True`` if the tessellation domain origin is upper-left, ``False`` if lower-left.

:type: bool
)");
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

  DOCUMENT(R"(A flag indicating if this buffer is active or not.

:type: bool
)");
  bool active = false;

  DOCUMENT(R"(The :class:`ResourceId` of the bound data buffer.

:type: ResourceId
)");
  ResourceId bufferResourceId;

  DOCUMENT(R"(The offset in bytes to the start of the data in the :data:`bufferResourceId`.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The size in bytes of the data buffer.

:type: int
)");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The :class:`ResourceId` of the buffer storing the counter value (if set).

:type: ResourceId
)");
  ResourceId counterBufferResourceId;

  DOCUMENT(R"(The offset in bytes to the counter in the :data:`counterBufferResourceId`.

:type: int
)");
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

  DOCUMENT(R"(The X co-ordinate of the render area.

:type: int
)");
  int32_t x = 0;
  DOCUMENT(R"(The Y co-ordinate of the render area.

:type: int
)");
  int32_t y = 0;
  DOCUMENT(R"(The width of the render area.

:type: int
)");
  int32_t width = 0;
  DOCUMENT(R"(The height of the render area.

:type: int
)");
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

:type: bool
)");
  bool discardRectanglesExclusive = true;

  DOCUMENT(R"(Whether depth clip range is set to [-1, 1] through VK_EXT_depth_clip_control.

:type: bool
)");
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

:type: bool
)");
  bool depthClampEnable = false;

  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clipped.

.. note::
  In Vulkan 1.0 this value was implicitly set to the opposite of :data:`depthClampEnable`, but with
  later extensions & versions it can be set independently.

:type: bool
)");
  bool depthClipEnable = true;

  DOCUMENT(R"(``True`` if primitives should be discarded during rasterization.

:type: bool
)");
  bool rasterizerDiscardEnable = false;

  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.

:type: bool
)");
  bool frontCCW = false;

  DOCUMENT(R"(The polygon :class:`FillMode`.

:type: FillMode
)");
  FillMode fillMode = FillMode::Solid;

  DOCUMENT(R"(The polygon :class:`CullMode`.

:type: CullMode
)");
  CullMode cullMode = CullMode::NoCull;

  DOCUMENT(R"(The active conservative rasterization mode.

:type: ConservativeRaster
)");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;

  DOCUMENT(R"(The extra size in pixels to increase primitives by during conservative rasterization,
in the x and y directions in screen space.

See :data:`conservativeRasterizationMode`

:type: float
)");
  float extraPrimitiveOverestimationSize = 0.0f;

  DOCUMENT(R"(Whether the provoking vertex is the first one (default behaviour).

:type: bool
)");
  bool provokingVertexFirst = true;

  DOCUMENT(R"(Whether depth biasing is enabled.

:type: bool
)");
  bool depthBiasEnable = false;

  DOCUMENT(R"(The fixed depth bias value to apply to z-values.

:type: float
)");
  float depthBias = 0.0f;

  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`

:type: float
)");
  float depthBiasClamp = 0.0f;

  DOCUMENT(R"(The slope-scaled depth bias value to apply to z-values.

:type: float
)");
  float slopeScaledDepthBias = 0.0f;

  DOCUMENT(R"(The fixed line width in pixels.

:type: float
)");
  float lineWidth = 0.0f;

  DOCUMENT(R"(The line rasterization mode.

:type: LineRaster
)");
  LineRaster lineRasterMode = LineRaster::Default;

  DOCUMENT(R"(The line stipple factor, or 0 if line stipple is disabled.

:type: int
)");
  uint32_t lineStippleFactor = 0;

  DOCUMENT(R"(The line stipple bit-pattern.

:type: int
)");
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

  DOCUMENT(R"(The width in pixels of the region configured.

:type: int
)");
  uint32_t gridWidth = 1;
  DOCUMENT(R"(The height in pixels of the region configured.

:type: int
)");
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

  DOCUMENT(R"(How many samples to use when rasterizing.

:type: int
)");
  uint32_t rasterSamples = 0;
  DOCUMENT(R"(``True`` if rendering should happen at sample-rate frequency.

:type: bool
)");
  bool sampleShadingEnable = false;
  DOCUMENT(R"(The minimum sample shading rate.

:type: float
)");
  float minSampleShading = 0.0f;
  DOCUMENT(R"(A mask that generated samples should be masked with using bitwise ``AND``.

:type: int
)");
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

  DOCUMENT(R"(``True`` if alpha-to-coverage should be used when blending to an MSAA target.

:type: bool
)");
  bool alphaToCoverageEnable = false;
  DOCUMENT(R"(``True`` if alpha-to-one should be used when blending to an MSAA target.

:type: bool
)");
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

  DOCUMENT(R"(``True`` if depth testing should be performed.

:type: bool
)");
  bool depthTestEnable = false;
  DOCUMENT(R"(``True`` if depth values should be written to the depth target.

:type: bool
)");
  bool depthWriteEnable = false;
  DOCUMENT(R"(``True`` if depth bounds tests should be applied.

:type: bool
)");
  bool depthBoundsEnable = false;
  DOCUMENT(R"(The :class:`CompareFunction` to use for testing depth values.

:type: CompareFunction
)");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;

  DOCUMENT(R"(``True`` if stencil operations should be performed.

:type: bool
)");
  bool stencilTestEnable = false;

  DOCUMENT(R"(The stencil state for front-facing polygons.

:type: StencilFace
)");
  StencilFace frontFace;

  DOCUMENT(R"(The stencil state for back-facing polygons.

:type: StencilFace
)");
  StencilFace backFace;

  DOCUMENT(R"(The near plane bounding value.

:type: float
)");
  float minDepthBounds = 0.0f;
  DOCUMENT(R"(The far plane bounding value.

:type: float
)");
  float maxDepthBounds = 0.0f;
};

DOCUMENT(R"(Describes the setup of a renderpass and subpasses.

.. data:: AttachmentUnused

  Alias for VK_ATTACHMENT_UNUSED, for use by the UI to know when a value in colorAttachmentLocations
  or colorAttachmentInputIndices is mapped to VK_ATTACHMENT_UNUSED.
)");
struct RenderPass
{
  DOCUMENT("");
  RenderPass() = default;
  RenderPass(const RenderPass &) = default;
  RenderPass &operator=(const RenderPass &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the render pass.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(Whether or not dynamic rendering is in use (no render pass or framebuffer objects).

:type: bool
)");
  bool dynamic = false;

  DOCUMENT(R"(Whether or not dynamic rendering is currently suspended.

:type: bool
)");
  bool suspended = false;

  DOCUMENT(R"(Whether or not there is a potential feedback loop.

:type: bool
)");
  bool feedbackLoop = false;

  DOCUMENT(R"(The index of the current active subpass.

:type: int
)");
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

:type: int
)");
  int32_t depthstencilAttachment = -1;
  DOCUMENT(R"(An index into the framebuffer attachments for the depth-stencil resolve attachment.

If there is no depth-stencil resolve attachment, this index is ``-1``.

:type: int
)");
  int32_t depthstencilResolveAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment density attachment.

If there is no fragment density attachment, this index is ``-1``.

.. note::
  Only one at most of :data:`fragmentDensityAttachment` and :data:`shadingRateAttachment` will be
  set.

:type: int
)");
  int32_t fragmentDensityAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment shading rate attachment.

If there is no fragment shading rate attachment, this index is ``-1``.

.. note::
  Only one at most of :data:`fragmentDensityAttachment` and :data:`shadingRateAttachment` will be
  set.

:type: int
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

:type: int
)");
  uint32_t tileOnlyMSAASampleCount = 0;

  DOCUMENT(R"(The color index->location mapping set up by dynamic rendering local read.

:type: List[int]
)");
  rdcarray<uint32_t> colorAttachmentLocations;

  DOCUMENT(R"(The color index->input index mapping set up by dynamic rendering local read.

:type: List[int]
)");
  rdcarray<uint32_t> colorAttachmentInputIndices;

  DOCUMENT(R"(Whether or not depth input attachment index is implicit (dynamic rendering).

:type: bool
)");
  bool isDepthInputAttachmentIndexImplicit = true;

  DOCUMENT(R"(Whether or not stencil  input attachment index is implicit (dynamic rendering).

:type: bool
)");
  bool isStencilInputAttachmentIndexImplicit = true;

  DOCUMENT(R"(Depth input attachment index if explicit (dynamic rendering).

:type: int
)");
  uint32_t depthInputAttachmentIndex = UINT32_MAX;

  DOCUMENT(R"(Stencil input attachment index if explicit (dynamic rendering).

:type: int
)");
  uint32_t stencilInputAttachmentIndex = UINT32_MAX;

  static const uint32_t AttachmentUnused = ~0U;
};

DOCUMENT("Describes a framebuffer object and its attachments.");
struct Framebuffer
{
  DOCUMENT("");
  Framebuffer() = default;
  Framebuffer(const Framebuffer &) = default;
  Framebuffer &operator=(const Framebuffer &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the framebuffer object.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The attachments of this framebuffer.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> attachments;

  DOCUMENT(R"(The width of this framebuffer in pixels.

:type: int
)");
  uint32_t width = 0;
  DOCUMENT(R"(The height of this framebuffer in pixels.

:type: int
)");
  uint32_t height = 0;
  DOCUMENT(R"(The number of layers in this framebuffer.

:type: int
)");
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

  DOCUMENT(R"(If feedback loops are allowed on color attachments

:type: bool
)");
  bool colorFeedbackAllowed = false;

  DOCUMENT(R"(If feedback loops are allowed on depth attachments

:type: bool
)");
  bool depthFeedbackAllowed = false;

  DOCUMENT(R"(If feedback loops are allowed on stencil attachments

:type: bool
)");
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
  DOCUMENT(R"(The first mip level used in the range.

:type: int
)");
  uint32_t baseMip = 0;
  DOCUMENT(R"(For 3D textures and texture arrays, the first slice used in the range.

:type: int
)");
  uint32_t baseLayer = 0;
  DOCUMENT(R"(The number of mip levels in the range.

:type: int
)");
  uint32_t numMip = 1;
  DOCUMENT(R"(For 3D textures and texture arrays, the number of array slices in the range.

:type: int
)");
  uint32_t numLayer = 1;
  DOCUMENT(R"(The name of the current image state.

:type: str
)");
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
  DOCUMENT(R"(The :class:`ResourceId` of the image.

:type: ResourceId
)");
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

  DOCUMENT(R"(The :class:`ResourceId` of the buffer containing the predicate for conditional rendering.

:type: ResourceId
)");
  ResourceId bufferId;

  DOCUMENT(R"(The byte offset into buffer where the predicate is located.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(``True`` if predicate result is inverted.

:type: bool
)");
  bool isInverted = false;

  DOCUMENT(R"(``True`` if the current predicate would render.

:type: bool
)");
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
DECLARE_REFLECTION_STRUCT(VKPipe::DescriptorBuffer);
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
