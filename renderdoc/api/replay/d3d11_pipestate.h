/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

// NOTE: Remember that python sees namespaces flattened to a prefix - i.e. D3D11Pipe::Layout is
// renamed to D3D11Layout, so these types must be referenced in the documentation

namespace D3D11Pipe
{
DOCUMENT(R"(Describes a single D3D11 input layout element for one vertex input.

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

  // D3D11_APPEND_ALIGNED_ELEMENT
  static const uint32_t TightlyPacked = ~0U;
};

DOCUMENT("Describes a single D3D11 vertex buffer binding.")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteStride == o.byteStride && byteOffset == o.byteOffset;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint32_t byteOffset = 0;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the D3D11 index buffer binding.")
struct IndexBuffer
{
  DOCUMENT("");
  IndexBuffer() = default;
  IndexBuffer(const IndexBuffer &) = default;
  IndexBuffer &operator=(const IndexBuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint32_t byteOffset = 0;

  DOCUMENT(R"(The number of bytes for each index in the index buffer. Typically 2 or 4 bytes but
it can be 0 if no index buffer is bound.
)");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the input assembler data.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;
  InputAssembly &operator=(const InputAssembly &) = default;

  DOCUMENT(R"(The input layout elements in this layout.

:type: List[D3D11Layout]
)");
  rdcarray<Layout> layouts;

  DOCUMENT("The :class:`ResourceId` of the layout object.");
  ResourceId resourceId;

  DOCUMENT(R"(The shader reflection for the bytecode used to create the input layout.

:type: ShaderReflection
)");
  ShaderReflection *bytecode = NULL;

  DOCUMENT(R"(The bound vertex buffers

:type: List[D3D11VertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT(R"(The bound index buffer.

:type: D3D11IndexBuffer
)");
  IndexBuffer indexBuffer;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;
};

DOCUMENT("Describes a D3D11 shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader itself.");
  ResourceId resourceId;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  ShaderReflection *reflection = NULL;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT(R"(The bound class instance names.

:type: List[str]
)");
  rdcarray<rdcstr> classInstances;
};

DOCUMENT("Describes a binding on the D3D11 stream-out stage.");
struct StreamOutBind
{
  DOCUMENT("");
  StreamOutBind() = default;
  StreamOutBind(const StreamOutBind &) = default;
  StreamOutBind &operator=(const StreamOutBind &) = default;

  bool operator==(const StreamOutBind &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset;
  }
  bool operator<(const StreamOutBind &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer.");
  ResourceId resourceId;

  DOCUMENT("The byte offset of the stream-output binding.");
  uint32_t byteOffset = 0;
};

DOCUMENT(R"(Describes the stream-out stage bindings.

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

:type: List[D3D11StreamOutBind]
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

DOCUMENT("Describes a rasterizer state object.");
struct RasterizerState
{
  DOCUMENT("");
  RasterizerState() = default;
  RasterizerState(const RasterizerState &) = default;
  RasterizerState &operator=(const RasterizerState &) = default;

  DOCUMENT("The :class:`ResourceId` of the rasterizer state object.");
  ResourceId resourceId;
  DOCUMENT("The polygon :class:`FillMode`.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon :class:`CullMode`.");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool frontCCW = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  int32_t depthBias = 0;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float depthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT("``True`` if pixels outside of the near and far depth planes should be clipped.");
  bool depthClip = false;
  DOCUMENT("``True`` if the scissor test should be applied.");
  bool scissorEnable = false;
  DOCUMENT("``True`` if the quadrilateral MSAA algorithm should be used on MSAA targets.");
  bool multisampleEnable = false;
  DOCUMENT(
      "``True`` if lines should be anti-aliased. Ignored if :data:`multisampleEnable` is "
      "``False``.");
  bool antialiasedLines = false;
  DOCUMENT(R"(A sample count to force rasterization to when UAV rendering or rasterizing, or 0 to
not force any sample count.
)");
  uint32_t forcedSampleCount = 0;
  DOCUMENT("The current :class:`ConservativeRaster` mode.");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;
};

DOCUMENT("Describes the rasterization state of the D3D11 pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  DOCUMENT(R"(The bound viewports.

:type: List[Viewport]
)");
  rdcarray<Viewport> viewports;

  DOCUMENT(R"(The bound scissor regions.

:type: List[Scissor]
)");
  rdcarray<Scissor> scissors;

  DOCUMENT(R"(The details of the rasterization state.

:type: D3D11RasterizerState
)");
  RasterizerState state;
};

DOCUMENT("Describes a depth-stencil state object.");
struct DepthStencilState
{
  DOCUMENT("");
  DepthStencilState() = default;
  DepthStencilState(const DepthStencilState &) = default;
  DepthStencilState &operator=(const DepthStencilState &) = default;

  DOCUMENT("The :class:`ResourceId` of the depth-stencil state object.");
  ResourceId resourceId;
  DOCUMENT("``True`` if depth testing should be performed.");
  bool depthEnable = false;
  DOCUMENT("The :class:`CompareFunction` to use for testing depth values.");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool depthWrites = false;
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
};

DOCUMENT("Describes a blend state object.");
struct BlendState
{
  DOCUMENT("");
  BlendState() = default;
  BlendState(const BlendState &) = default;
  BlendState &operator=(const BlendState &) = default;

  DOCUMENT("The :class:`ResourceId` of the blend state object.");
  ResourceId resourceId;

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
  DOCUMENT("The mask determining which samples are written to.");
  uint32_t sampleMask = ~0U;
};

DOCUMENT("Describes the current state of the output-merger stage of the D3D11 pipeline.");
struct OutputMerger
{
  DOCUMENT("");
  OutputMerger() = default;
  OutputMerger(const OutputMerger &) = default;
  OutputMerger &operator=(const OutputMerger &) = default;

  DOCUMENT(R"(The current depth-stencil state details.

:type: D3D11DepthStencilState
)");
  DepthStencilState depthStencilState;

  DOCUMENT(R"(The current blend state details.

:type: D3D11BlendState
)");
  BlendState blendState;

  DOCUMENT(R"(The bound render targets.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> renderTargets;

  DOCUMENT("Which slot in the output targets is the first UAV.");
  uint32_t uavStartSlot = 0;

  DOCUMENT(R"(The currently bound depth-stencil target.

:type: Descriptor
)");
  Descriptor depthTarget;
  DOCUMENT("``True`` if depth access to the depth-stencil target is read-only.");
  bool depthReadOnly = false;
  DOCUMENT("``True`` if stencil access to the depth-stencil target is read-only.");
  bool stencilReadOnly = false;
};

DOCUMENT("Describes the current state of predicated rendering.");
struct Predication
{
  DOCUMENT("");
  Predication() = default;
  Predication(const Predication &) = default;
  Predication &operator=(const Predication &) = default;

  DOCUMENT("The :class:`ResourceId` of the active predicate.");
  ResourceId resourceId;

  DOCUMENT("The value to go along with the predicate.");
  bool value = false;

  DOCUMENT("``True`` if the current predicate would render.");
  bool isPassing = false;
};

DOCUMENT("The full current D3D11 pipeline state.");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT(R"(The input assembly pipeline stage.

:type: D3D11InputAssembly
)");
  InputAssembly inputAssembly;

  DOCUMENT(R"(The vertex shader stage.

:type: D3D11Shader
)");
  Shader vertexShader;
  DOCUMENT(R"(The hull shader stage.

:type: D3D11Shader
)");
  Shader hullShader;
  DOCUMENT(R"(The domain shader stage.

:type: D3D11Shader
)");
  Shader domainShader;
  DOCUMENT(R"(The geometry shader stage.

:type: D3D11Shader
)");
  Shader geometryShader;
  DOCUMENT(R"(The pixel shader stage.

:type: D3D11Shader
)");
  Shader pixelShader;
  DOCUMENT(R"(The compute shader stage.

:type: D3D11Shader
)");
  Shader computeShader;

  DOCUMENT(R"(The virtual descriptor storage.

:type: ResourceId
)");
  ResourceId descriptorStore;

  DOCUMENT(R"(The number of descriptors in the virtual descriptor storage.

:type: int
)");
  uint32_t descriptorCount = 0;

  DOCUMENT(R"(The byte size of a descriptor in the virtual descriptor storage.

:type: int
)");
  uint32_t descriptorByteSize = 0;

  DOCUMENT(R"(The stream-out pipeline stage.

:type: D3D11StreamOut
)");
  StreamOut streamOut;

  DOCUMENT(R"(The rasterizer pipeline stage.

:type: D3D11Rasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The output merger pipeline stage.

:type: D3D11OutputMerger
)");
  OutputMerger outputMerger;

  DOCUMENT(R"(The predicated rendering state.

:type: D3D11Predication
)");
  Predication predication;
};

};    // namespace D3D11Pipe

DECLARE_REFLECTION_STRUCT(D3D11Pipe::Layout);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::IndexBuffer);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::InputAssembly);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Shader);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::StreamOutBind);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::StreamOut);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::DepthStencilState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::OutputMerger);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Predication);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::State);
