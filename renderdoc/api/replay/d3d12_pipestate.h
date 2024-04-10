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

namespace D3D12Pipe
{
DOCUMENT(R"(Describes a single D3D12 input layout element for one vertex input.

.. data:: TightlyPacked

  Value for :data:`byteOffset` that indicates this element is tightly packed.
)");
struct Layout
{
  DOCUMENT("");
  Layout() = default;
  Layout(const Layout &) = default;
  Layout &operator=(const Layout &) = default;

  bool operator==(const Layout &o) const
  {
    return semanticName == o.semanticName && semanticIndex == o.semanticIndex &&
           format == o.format && inputSlot == o.inputSlot && byteOffset == o.byteOffset &&
           perInstance == o.perInstance && instanceDataStepRate == o.instanceDataStepRate;
  }
  bool operator<(const Layout &o) const
  {
    if(!(semanticName == o.semanticName))
      return semanticName < o.semanticName;
    if(!(semanticIndex == o.semanticIndex))
      return semanticIndex < o.semanticIndex;
    if(!(format == o.format))
      return format < o.format;
    if(!(inputSlot == o.inputSlot))
      return inputSlot < o.inputSlot;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(perInstance == o.perInstance))
      return perInstance < o.perInstance;
    if(!(instanceDataStepRate == o.instanceDataStepRate))
      return instanceDataStepRate < o.instanceDataStepRate;
    return false;
  }
  DOCUMENT("The semantic name for this input.");
  rdcstr semanticName;

  DOCUMENT("The semantic index for this input.");
  uint32_t semanticIndex = 0;

  DOCUMENT(R"(The format describing how the input data is interpreted.

:type: ResourceFormat
)");
  ResourceFormat format;

  DOCUMENT("The vertex buffer input slot where the data is sourced from.");
  uint32_t inputSlot = 0;

  DOCUMENT(R"(The byte offset from the start of the vertex data in the vertex buffer from
:data:`inputSlot`.

If the value is :data:`TightlyPacked` then the element is packed tightly after the previous element, or 0
if this is the first element.
)");
  uint32_t byteOffset = 0;

  DOCUMENT("``True`` if the vertex data is instance-rate.");
  bool perInstance = false;

  DOCUMENT(R"(If :data:`perInstance` is ``True`` then this is how many times each instance data is
used before advancing to the next instance.

E.g. if this value is two, then two instances will be drawn with the first instance data, then two
with the next instance data.
)");
  uint32_t instanceDataStepRate = 0;

  // D3D12_APPEND_ALIGNED_ELEMENT
  static const uint32_t TightlyPacked = ~0U;
};

DOCUMENT("Describes a single D3D12 vertex buffer binding.")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteStride == o.byteStride && byteSize == o.byteSize &&
           byteOffset == o.byteOffset;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint64_t byteOffset = 0;

  DOCUMENT("The number of bytes available in this vertex buffer.");
  uint32_t byteSize = 0;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the D3D12 index buffer binding.")
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

  DOCUMENT("The number of bytes available in this index buffer.");
  uint32_t byteSize = 0;

  DOCUMENT(R"(The number of bytes for each index in the index buffer. Typically 2 or 4 bytes but
it can be 0 if no index buffer is bound.
)");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the input assembler state in the PSO.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;
  InputAssembly &operator=(const InputAssembly &) = default;

  DOCUMENT(R"(The input layout elements in this layout.

:type: List[D3D12Layout]
)");
  rdcarray<Layout> layouts;

  DOCUMENT(R"(The bound vertex buffers

:type: List[D3D12VertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT(R"(The bound index buffer.

:type: D3D12IndexBuffer
)");
  IndexBuffer indexBuffer;

  DOCUMENT(R"(The index value to use for cutting strips. Either ``0``, ``0xffff`` or ``0xffffffff``.
If the value is 0, strip cutting is disabled.
)");
  uint32_t indexStripCutValue = 0;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;
};

DOCUMENT("Describes a D3D12 shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader object itself.");
  ResourceId resourceId;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  ShaderReflection *reflection = NULL;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;
};

DOCUMENT("Describes a binding on the D3D12 stream-out stage.");
struct StreamOutBind
{
  DOCUMENT("");
  StreamOutBind() = default;
  StreamOutBind(const StreamOutBind &) = default;
  StreamOutBind &operator=(const StreamOutBind &) = default;

  bool operator==(const StreamOutBind &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteSize == o.byteSize &&
           writtenCountResourceId == o.writtenCountResourceId &&
           writtenCountByteOffset == o.writtenCountByteOffset;
  }
  bool operator<(const StreamOutBind &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(writtenCountResourceId == o.writtenCountResourceId))
      return writtenCountResourceId < o.writtenCountResourceId;
    if(!(writtenCountByteOffset == o.writtenCountByteOffset))
      return writtenCountByteOffset < o.writtenCountByteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer.");
  ResourceId resourceId;
  DOCUMENT(R"(The byte offset in :data:`resourceId` where the buffer view starts in the underlying
buffer.
)");
  uint64_t byteOffset = 0;
  DOCUMENT("How many bytes are in this stream-out buffer view.");
  uint64_t byteSize = 0;

  DOCUMENT("The :class:`ResourceId` of the buffer where the written count will be stored.");
  ResourceId writtenCountResourceId;
  DOCUMENT(R"(The byte offset in :data:`writtenCountResourceId` where the stream-out count will be
written.
)");
  uint64_t writtenCountByteOffset = 0;
};

DOCUMENT(R"(Describes the stream-out state in the PSO.

.. data:: NoRasterization

  Value for :data:`rasterizedStream` that indicates no stream is being rasterized.
)");
struct StreamOut
{
  DOCUMENT("");
  StreamOut() = default;
  StreamOut(const StreamOut &) = default;
  StreamOut &operator=(const StreamOut &) = default;

  DOCUMENT(R"(The bound stream-out buffer bindings.

:type: List[D3D12StreamOutBind]
)");
  rdcarray<StreamOutBind> outputs;

  DOCUMENT(R"(Which stream-out stream is being used for rasterization.

If the value is :data:`NoRasterization` then no stream has been selected for rasterization.

:type: int
)");
  uint32_t rasterizedStream = 0;

  // D3D11_SO_NO_RASTERIZED_STREAM
  static const uint32_t NoRasterization = ~0U;
};

DOCUMENT("Describes the rasterizer state in the PSO.");
struct RasterizerState
{
  DOCUMENT("");
  RasterizerState() = default;
  RasterizerState(const RasterizerState &) = default;
  RasterizerState &operator=(const RasterizerState &) = default;

  DOCUMENT("The polygon :class:`FillMode`.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon :class:`CullMode`.");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool frontCCW = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  float depthBias = 0.0f;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float depthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT("``True`` if pixels outside of the near and far depth planes should be clipped.");
  bool depthClip = false;
  DOCUMENT("The line rasterization mode.");
  LineRaster lineRasterMode = LineRaster::Default;
  DOCUMENT(R"(A sample count to force rasterization to when UAV rendering or rasterizing, or 0 to
not force any sample count.
)");
  uint32_t forcedSampleCount = 0;
  DOCUMENT("The current :class:`ConservativeRaster` mode.");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;
  DOCUMENT(R"(The current base variable shading rate. This will always be 1x1 when variable shading
is disabled.

:type: Tuple[int,int]
)");
  rdcpair<uint32_t, uint32_t> baseShadingRate = {1, 1};
  DOCUMENT(R"(The shading rate combiners.

The combiners are applied as follows, according to the D3D spec:

  ``intermediateRate = combiner[0] ( baseShadingRate,  shaderExportedShadingRate )``
  ``finalRate        = combiner[1] ( intermediateRate, imageBasedShadingRate     )``

Where the first input is from :data:`baseShadingRate` and the second is the exported shading rate
from a vertex or geometry shader, which defaults to 1x1 if not exported.

The intermediate result is then used as the first input to the second combiner, together with the
shading rate sampled from the shading rate image.

:type: Tuple[ShadingRateCombiner,ShadingRateCombiner]
)");
  rdcpair<ShadingRateCombiner, ShadingRateCombiner> shadingRateCombiners = {
      ShadingRateCombiner::Passthrough, ShadingRateCombiner::Passthrough};
  DOCUMENT(R"(The image bound as a shading rate image.

:type: ResourceId
)");
  ResourceId shadingRateImage;
};

DOCUMENT("Describes the rasterization state of the D3D12 pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  DOCUMENT("The mask determining which samples are written to.");
  uint32_t sampleMask = ~0U;

  DOCUMENT(R"(The bound viewports.

:type: List[Viewport]
)");
  rdcarray<Viewport> viewports;

  DOCUMENT(R"(The bound scissor regions.

:type: List[Scissor]
)");
  rdcarray<Scissor> scissors;

  DOCUMENT(R"(The details of the rasterization state.

:type: D3D12RasterizerState
)");
  RasterizerState state;
};

DOCUMENT("Describes the state of the depth-stencil state in the PSO.");
struct DepthStencilState
{
  DOCUMENT("");
  DepthStencilState() = default;
  DepthStencilState(const DepthStencilState &) = default;
  DepthStencilState &operator=(const DepthStencilState &) = default;

  DOCUMENT("``True`` if depth testing should be performed.");
  bool depthEnable = false;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool depthWrites = false;
  DOCUMENT("``True`` if depth bounds tests should be applied.");
  bool depthBoundsEnable = false;
  DOCUMENT("The :class:`CompareFunction` to use for testing depth values.");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("``True`` if stencil operations should be performed.");
  bool stencilEnable = false;

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

DOCUMENT("Describes the blend state in the PSO.");
struct BlendState
{
  DOCUMENT("");
  BlendState() = default;
  BlendState(const BlendState &) = default;
  BlendState &operator=(const BlendState &) = default;

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverage = false;
  DOCUMENT(R"(``True`` if independent blending for each target should be used.

``False`` if the first blend should be applied to all targets.
)");
  bool independentBlend = false;

  DOCUMENT(R"(The blend operations for each target.

:type: List[ColorBlend]
)");
  rdcarray<ColorBlend> blends;

  DOCUMENT(R"(The constant blend factor to use in blend equations.

:type: Tuple[float,float,float,float]
)");
  rdcfixedarray<float, 4> blendFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the current state of the output-merger stage of the D3D12 pipeline.");
struct OM
{
  DOCUMENT("");
  OM() = default;
  OM(const OM &) = default;
  OM &operator=(const OM &) = default;

  DOCUMENT(R"(The current depth-stencil state details.

:type: D3D12DepthStencilState
)");
  DepthStencilState depthStencilState;

  DOCUMENT(R"(The current blend state details.

:type: D3D12BlendState
)");
  BlendState blendState;

  DOCUMENT(R"(The bound render targets.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> renderTargets;

  DOCUMENT(R"(The currently bound depth-stencil target.

:type: Descriptor
)");
  Descriptor depthTarget;
  DOCUMENT("``True`` if depth access to the depth-stencil target is read-only.");
  bool depthReadOnly = false;
  DOCUMENT("``True`` if stenncil access to the depth-stencil target is read-only.");
  bool stencilReadOnly = false;

  DOCUMENT("The sample count used for rendering.");
  uint32_t multiSampleCount = 1;
  DOCUMENT("The MSAA quality level used for rendering.");
  uint32_t multiSampleQuality = 0;
};

DOCUMENT("Describes the current state that a sub-resource is in.");
struct ResourceState
{
  DOCUMENT("");
  ResourceState() = default;
  ResourceState(const ResourceState &) = default;
  ResourceState &operator=(const ResourceState &) = default;

  bool operator==(const ResourceState &o) const { return name == o.name; }
  bool operator<(const ResourceState &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    return false;
  }
  DOCUMENT("A human-readable name for the current state.");
  rdcstr name;
};

DOCUMENT("Contains the current state of a given resource.");
struct ResourceData
{
  DOCUMENT("");
  ResourceData() = default;
  ResourceData(const ResourceData &) = default;
  ResourceData &operator=(const ResourceData &) = default;

  bool operator==(const ResourceData &o) const
  {
    return resourceId == o.resourceId && states == o.states;
  }
  bool operator<(const ResourceData &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(states == o.states))
      return states < o.states;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the resource.");
  ResourceId resourceId;

  DOCUMENT(R"(The subresource states in this resource.

:type: List[D3D12ResourceState]
)");
  rdcarray<ResourceState> states;
};

DOCUMENT("Contains the structure of a single range within a root table definition.");
struct RootTableRange
{
  DOCUMENT("");
  RootTableRange() = default;
  RootTableRange(const RootTableRange &) = default;
  RootTableRange &operator=(const RootTableRange &) = default;

  bool operator==(const RootTableRange &o) const
  {
    return category == o.category && space == o.space && baseRegister == o.baseRegister &&
           count == o.count && tableByteOffset == o.tableByteOffset;
  }
  bool operator<(const RootTableRange &o) const
  {
    if(!(category == o.category))
      return category < o.category;
    if(!(space == o.space))
      return space < o.space;
    if(!(baseRegister == o.baseRegister))
      return baseRegister < o.baseRegister;
    if(!(count == o.count))
      return count < o.count;
    if(!(tableByteOffset == o.tableByteOffset))
      return tableByteOffset < o.tableByteOffset;
    return false;
  }
  DOCUMENT(R"(The descriptor category specified in this range.

:type: DescriptorCategory
)");
  DescriptorCategory category = DescriptorCategory::Unknown;

  DOCUMENT(R"(The register space of this range.

:type: int
)");
  uint32_t space = 0;

  DOCUMENT(R"(The first register in this range.

:type: int
)");
  uint32_t baseRegister = 0;

  DOCUMENT(R"(The number of registers in this range.

:type: int
)");
  uint32_t count = 0;

  DOCUMENT(R"(The offset in bytes from the start of the table as defined in :class:`D3D12RootParam`.

:type: int
)");
  uint32_t tableByteOffset = 0;

  DOCUMENT(R"(Whether or not this table was appended after the previous, leading to an auto-calculated
offset in :data:`tableByteOffset`.

:type: bool
)");
  bool appended = false;
};

DOCUMENT("Contains the structure and content of a single root parameter.");
struct RootParam
{
  DOCUMENT("");
  RootParam() = default;
  RootParam(const RootParam &) = default;
  RootParam &operator=(const RootParam &) = default;

  bool operator==(const RootParam &o) const
  {
    return visibility == o.visibility && heap == o.heap && heapByteOffset == o.heapByteOffset &&
           tableRanges == o.tableRanges && descriptor == o.descriptor && constants == o.constants;
  }
  bool operator<(const RootParam &o) const
  {
    if(!(visibility == o.visibility))
      return visibility < o.visibility;
    if(!(heap == o.heap))
      return heap < o.heap;
    if(!(heapByteOffset == o.heapByteOffset))
      return heapByteOffset < o.heapByteOffset;
    if(!(tableRanges == o.tableRanges))
      return tableRanges < o.tableRanges;
    if(!(descriptor == o.descriptor))
      return descriptor < o.descriptor;
    if(!(constants == o.constants))
      return constants < o.constants;
    return false;
  }

  DOCUMENT(R"(The shader stage that can access this parameter.

:type: ShaderStageMask
)");
  ShaderStageMask visibility;

  DOCUMENT(R"(For a root constant parameter, the words defined.

:type: bytes
)");
  bytebuf constants;

  DOCUMENT(R"(For a root descriptor parameter, the descriptor itself.

:type: Descriptor
)");
  Descriptor descriptor;

  DOCUMENT(R"(For a root table parameter, the descriptor heap bound to this parameter. See
:data:`heapByteOffset` and :data:`tableRanges`.

:type: ResourceId
)");
  ResourceId heap;

  DOCUMENT(R"(For a root table parameter, the byte offset into the descriptor heap bound to this
parameter. See :data:`heap` and :data:`tableRanges`.

:type: ResourceId
)");
  uint32_t heapByteOffset = 0;

  DOCUMENT(R"(For a root table parameter, the descriptor ranges that define this table. See
:data:`heap` and :data:`heapByteOffset`.

:type: List[D3D12RootTableRange]
)");
  rdcarray<RootTableRange> tableRanges;
};

DOCUMENT("Contains the details of a single static sampler in a root signature.");
struct StaticSampler
{
  DOCUMENT("");
  StaticSampler() = default;
  StaticSampler(const StaticSampler &) = default;
  StaticSampler &operator=(const StaticSampler &) = default;

  bool operator==(const StaticSampler &o) const
  {
    return visibility == o.visibility && space == o.space && reg == o.reg &&
           descriptor == o.descriptor;
  }
  bool operator<(const StaticSampler &o) const
  {
    if(!(visibility == o.visibility))
      return visibility < o.visibility;
    if(!(space == o.space))
      return space < o.space;
    if(!(reg == o.reg))
      return reg < o.reg;
    if(!(descriptor == o.descriptor))
      return descriptor < o.descriptor;
    return false;
  }

  DOCUMENT(R"(The shader stage that can access this sampler.

:type: ShaderStageMask
)");
  ShaderStageMask visibility;

  DOCUMENT(R"(The register space of this sampler.

:type: int
)");
  uint32_t space = 0;

  DOCUMENT(R"(The register number of this sampler.

:type: int
)");
  uint32_t reg = 0;

  DOCUMENT(R"(The details of the sampler descriptor itself.

:type: SamplerDescriptor
)");
  SamplerDescriptor descriptor;
};

DOCUMENT("Contains the root signature structure and root parameters.");
struct RootSignature
{
  DOCUMENT("");
  RootSignature() = default;
  RootSignature(const RootSignature &) = default;
  RootSignature &operator=(const RootSignature &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the root signature object.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The parameters in this root signature.
    
:type: List[D3D12RootParam]
)");
  rdcarray<RootParam> parameters;

  DOCUMENT(R"(The static samplers defined in this root signature.
    
:type: List[D3D12StaticSampler]
)");
  rdcarray<StaticSampler> staticSamplers;
};

DOCUMENT("The full current D3D12 pipeline state.");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT("The :class:`ResourceId` of the pipeline state object.");
  ResourceId pipelineResourceId;

  DOCUMENT(R"(The descriptor heaps currently bound.
    
:type: List[ResourceId]
)");
  rdcarray<ResourceId> descriptorHeaps;

  DOCUMENT(R"(Details of the root signature structure and root parameters.
    
:type: D3D12RootSignature
)");
  RootSignature rootSignature;

  DOCUMENT(R"(The input assembly pipeline stage.

:type: D3D12InputAssembly
)");
  InputAssembly inputAssembly;

  DOCUMENT(R"(The vertex shader stage.

:type: D3D12Shader
)");
  Shader vertexShader;
  DOCUMENT(R"(The hull shader stage.

:type: D3D12Shader
)");
  Shader hullShader;
  DOCUMENT(R"(The domain shader stage.

:type: D3D12Shader
)");
  Shader domainShader;
  DOCUMENT(R"(The geometry shader stage.

:type: D3D12Shader
)");
  Shader geometryShader;
  DOCUMENT(R"(The pixel shader stage.

:type: D3D12Shader
)");
  Shader pixelShader;
  DOCUMENT(R"(The compute shader stage.

:type: D3D12Shader
)");
  Shader computeShader;
  DOCUMENT(R"(The amplification shader stage.

:type: D3D12Shader
)");
  Shader ampShader;
  DOCUMENT(R"(The mesh shader stage.

:type: D3D12Shader
)");
  Shader meshShader;

  DOCUMENT(R"(The stream-out pipeline stage.

:type: D3D12StreamOut
)");
  StreamOut streamOut;

  DOCUMENT(R"(The rasterizer pipeline stage.

:type: D3D12Rasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The output merger pipeline stage.

:type: D3D12OM
)");
  OM outputMerger;

  DOCUMENT(R"(The resource states for the currently live resources.

:type: List[D3D12ResourceData]
)");
  rdcarray<ResourceData> resourceStates;
};

};    // namespace D3D12Pipe

DECLARE_REFLECTION_STRUCT(D3D12Pipe::Layout);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::IndexBuffer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::InputAssembly);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Shader);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StreamOutBind);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StreamOut);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::DepthStencilState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::OM);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceData);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RootTableRange);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RootParam);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StaticSampler);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RootSignature);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::State);
