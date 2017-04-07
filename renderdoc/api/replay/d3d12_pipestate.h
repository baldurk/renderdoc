/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "shader_types.h"

namespace D3D12Pipe
{
DOCUMENT(R"(Describes a single D3D12 input layout element for one vertex input.

.. data:: TightlyPacked

  Value for :data:`ByteOffset` that indicates this element is tightly packed.
)");
struct Layout
{
  DOCUMENT("The semantic name for this input.");
  rdctype::str SemanticName;

  DOCUMENT("The semantic index for this input.");
  uint32_t SemanticIndex = 0;

  DOCUMENT("The :class:`ResourceFormat` describing how the input data is interpreted.");
  ResourceFormat Format;

  DOCUMENT("The vertex buffer input slot where the data is sourced from.");
  uint32_t InputSlot = 0;

  DOCUMENT(R"(The byte offset from the start of the vertex data in the vertex buffer from
:data:`InputSlot`.

If the value is :data:`TightlyPacked` then the element is packed tightly after the previous element, or 0
if this is the first element.
)");
  uint32_t ByteOffset = 0;

  DOCUMENT("``True`` if the vertex data is instance-rate.");
  bool32 PerInstance = false;

  DOCUMENT(R"(If :data:`PerInstance` is ``True`` then this is how many times each instance data is
used before advancing to the next instance.

E.g. if this value is two, then two instances will be drawn with the first instance data, then two
with the next instance data.
)");
  uint32_t InstanceDataStepRate = 0;

  // D3D12_APPEND_ALIGNED_ELEMENT
  static const uint32_t TightlyPacked = ~0U;
};

DOCUMENT("Describes a single D3D12 vertex buffer binding.")
struct VB
{
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId Buffer;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint64_t Offset = 0;

  DOCUMENT("The number of bytes available in this vertex buffer.");
  uint32_t Size = 0;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t Stride = 0;
};

DOCUMENT("Describes the D3D12 index buffer binding.")
struct IB
{
  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId Buffer;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint64_t Offset = 0;

  DOCUMENT("The number of bytes available in this index buffer.");
  uint32_t Size = 0;
};

DOCUMENT("Describes the input assembler state in the PSO.");
struct IA
{
  DOCUMENT("A list of :class:`D3D12_Layout` describing the input layout elements in this layout.");
  rdctype::array<Layout> layouts;

  DOCUMENT("A list of :class:`D3D12_VB` with the vertex buffers that are bound.");
  rdctype::array<VB> vbuffers;

  DOCUMENT("The :class:`D3D12_IB` describing the index buffer.");
  IB ibuffer;

  DOCUMENT(R"(The index value to use for cutting strips. Either ``0``, ``0xffff`` or ``0xffffffff``.
If the value is 0, strip cutting is disabled.
)");
  uint32_t indexStripCutValue = 0;
};

// Immediate indicates either a root parameter (not in a table), or static samplers
// RootElement is the index in the original root signature that this descriptor came from.

DOCUMENT("Describes the details of a D3D12 resource view - any one of UAV, SRV, RTV or DSV.");
struct View
{
  DOCUMENT("``True`` if this view is a root parameter (i.e. not in a table).");
  bool32 Immediate = false;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t RootElement = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t TableIndex = ~0U;

  DOCUMENT("The :class:`ResourceId` of the underlying resource the view refers to.");
  ResourceId Resource;
  DOCUMENT("The :class:`TextureDim` of the view type.");
  TextureDim Type;
  DOCUMENT("The :class:`ResourceFormat` that the view uses.");
  ResourceFormat Format;

  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  DOCUMENT("The :class:`D3DBufferViewFlags` set for the buffer.");
  D3DBufferViewFlags BufferFlags = D3DBufferViewFlags::NoFlags;
  DOCUMENT("If the view has a hidden counter, this stores the current value of the counter.");
  uint32_t BufferStructCount = 0;
  DOCUMENT(R"(The byte size of a single element in the view. Either the byte size of :data:`Format`,
or the structured buffer element size, as appropriate.
)");
  uint32_t ElementSize = 0;
  DOCUMENT("Valid for buffers - the first element to be used in the view.");
  uint64_t FirstElement = 0;
  DOCUMENT("Valid for buffers - the number of elements to be used in the view.");
  uint32_t NumElements = 1;

  DOCUMENT("The :class:`ResourceId` of the resource where the hidden buffer counter is stored.");
  ResourceId CounterResource;
  DOCUMENT("The byte offset in :data:`CounterResource` where the counter is stored.");
  uint64_t CounterByteOffset = 0;

  DOCUMENT("Valid for textures - the highest mip that is available through the view.");
  uint32_t HighestMip = 0;
  DOCUMENT("Valid for textures - the number of mip levels in the view.");
  uint32_t NumMipLevels = 1;

  DOCUMENT("Valid for texture arrays or 3D textures - the number of slices in the view.");
  uint32_t ArraySize = 1;
  DOCUMENT("Valid for texture arrays or 3D textures - the first slice available through the view.");
  uint32_t FirstArraySlice = 0;

  DOCUMENT("The minimum mip-level clamp applied when sampling this texture.");
  float MinLODClamp = 0.0f;
};

DOCUMENT("Describes the details of a sampler descriptor.");
struct Sampler
{
  DOCUMENT("``True`` if this view is a static sampler (i.e. not in a table).");
  bool32 Immediate = 0;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t RootElement = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t TableIndex = ~0U;

  DOCUMENT("The :class:`AddressMode` in the U direction.");
  AddressMode AddressU = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the V direction.");
  AddressMode AddressV = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the W direction.");
  AddressMode AddressW = AddressMode::Wrap;
  DOCUMENT("The RGBA border color.");
  float BorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT("The :class:`CompareFunc` for comparison samplers.");
  CompareFunc Comparison = CompareFunc::AlwaysTrue;
  DOCUMENT("The :class:`TextureFilter` describing the filtering mode.");
  TextureFilter Filter;
  DOCUMENT("The maximum anisotropic filtering level to use.");
  uint32_t MaxAniso = 0;
  DOCUMENT("The maximum mip level that can be used.");
  float MaxLOD = 0.0f;
  DOCUMENT("The minimum mip level that can be used.");
  float MinLOD = 0.0f;
  DOCUMENT("A bias to apply to the calculated mip level before sampling.");
  float MipLODBias = 0.0f;

  DOCUMENT(R"(Check if the border color is used in this D3D12 sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: bool
)");
  bool UseBorder() const
  {
    return AddressU == AddressMode::ClampBorder || AddressV == AddressMode::ClampBorder ||
           AddressW == AddressMode::ClampBorder;
  }
};

DOCUMENT("Describes the details of a constant buffer view descriptor.");
struct CBuffer
{
  DOCUMENT("``True`` if this view is a root constant (i.e. not in a table).");
  bool32 Immediate = false;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t RootElement = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t TableIndex = ~0U;

  DOCUMENT("The :class:`ResourceId` of the underlying buffer resource.");
  ResourceId Buffer;
  DOCUMENT("The byte offset where the buffer view starts in the underlying buffer.");
  uint64_t Offset = 0;
  DOCUMENT("How many bytes are in this constant buffer view.");
  uint32_t ByteSize = 0;

  DOCUMENT(R"(If :data:`Immediate` is ``True`` and this is a root constant, this contains a list of
``int`` values with the root values set.
)");
  rdctype::array<uint32_t> RootValues;
};

DOCUMENT("Contains all of the registers in a single register space mapped to by a root signature.");
struct RegisterSpace
{
  DOCUMENT("List of :class:`D3D12_CBuffer` containing the constant buffers.");
  rdctype::array<CBuffer> ConstantBuffers;
  DOCUMENT("List of :class:`D3D12_Sampler` containing the samplers.");
  rdctype::array<Sampler> Samplers;
  DOCUMENT("List of :class:`D3D12_View` containing the SRVs.");
  rdctype::array<View> SRVs;
  DOCUMENT("List of :class:`D3D12_View` containing the UAVs.");
  rdctype::array<View> UAVs;
};

DOCUMENT("Describes a D3D12 shader stage.");
struct Shader
{
  DOCUMENT("The :class:`ResourceId` of the shader object itself.");
  ResourceId Object;

  DOCUMENT("A :class:`ShaderReflection` describing the reflection data for this shader.");
  ShaderReflection *ShaderDetails = NULL;
  DOCUMENT(R"(A :class:`ShaderBindpointMapping` to match :data:`ShaderDetails` with the bindpoint
mapping data.
)");
  ShaderBindpointMapping BindpointMapping;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT("A list of :class:`D3D12_RegisterSpace` with the register spaces for this stage.");
  rdctype::array<RegisterSpace> Spaces;
};

DOCUMENT("Describes a binding on the D3D12 stream-out stage.");
struct SOBind
{
  DOCUMENT("The :class:`ResourceId` of the buffer.");
  ResourceId Buffer;
  DOCUMENT(
      "The byte offset in :data:`Buffer` where the buffer view starts in the underlying buffer.");
  uint64_t Offset = 0;
  DOCUMENT("How many bytes are in this stream-out buffer view.");
  uint64_t Size = 0;

  DOCUMENT("The :class:`ResourceId` of the buffer where the written count will be stored.");
  ResourceId WrittenCountBuffer;
  DOCUMENT(
      "The byte offset in :data:`WrittenCountBuffer` where the stream-out count will be written.");
  uint64_t WrittenCountOffset = 0;
};

DOCUMENT("Describes the stream-out state in the PSO.");
struct Streamout
{
  DOCUMENT("A list of ``D3D12_SOBind`` with the bound buffers.");
  rdctype::array<SOBind> Outputs;
};

DOCUMENT("Describes a single D3D12 viewport.");
struct Viewport
{
  Viewport() = default;
  Viewport(float TX, float TY, float W, float H, float MN, float MX)
      : X(TX), Y(TY), Width(W), Height(H), MinDepth(MN), MaxDepth(MX)
  {
  }

  DOCUMENT("Top-left X co-ordinate of the viewport.");
  float X = 0.0f;
  DOCUMENT("Top-left Y co-ordinate of the viewport.");
  float Y = 0.0f;
  DOCUMENT("The width of the viewport.");
  float Width = 0.0f;
  DOCUMENT("The height of the viewport.");
  float Height = 0.0f;
  DOCUMENT("The minimum depth of the viewport.");
  float MinDepth = 0.0f;
  DOCUMENT("The maximum depth of the viewport.");
  float MaxDepth = 0.0f;
};

DOCUMENT("Describes a single D3D12 scissor rect.");
struct Scissor
{
  Scissor() = default;
  Scissor(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
  DOCUMENT("Top-left X co-ordinate of the viewport.");
  int32_t left = 0;
  DOCUMENT("Top-left Y co-ordinate of the viewport.");
  int32_t top = 0;
  DOCUMENT("Bottom-right X co-ordinate of the viewport.");
  int32_t right = 0;
  DOCUMENT("Bottom-right Y co-ordinate of the viewport.");
  int32_t bottom = 0;
};

DOCUMENT("Describes the rasterizer state in the PSO.");
struct RasterizerState
{
  DOCUMENT("The polygon fill mode.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon culling mode.");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool32 FrontCCW = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  int32_t DepthBias = 0;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`DepthBias` and
:data:`SlopeScaledDepthBias`
)");
  float DepthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float SlopeScaledDepthBias = 0.0f;
  DOCUMENT("``True`` if pixels outside of the near and far depth planes should be clipped.");
  bool32 DepthClip = false;
  DOCUMENT("``True`` if the quadrilateral MSAA algorithm should be used on MSAA targets.");
  bool32 MultisampleEnable = false;
  DOCUMENT(
      "``True`` if lines should be anti-aliased. Ignored if :data:`MultisampleEnable` is "
      "``False``.");
  bool32 AntialiasedLineEnable = false;
  DOCUMENT(R"(A sample count to force rasterization to when UAV rendering or rasterizing, or 0 to
not force any sample count.
)");
  uint32_t ForcedSampleCount = 0;
  DOCUMENT("``True`` if a conservative rasterization algorithm should be used.");
  bool32 ConservativeRasterization = false;
};

DOCUMENT("Describes the rasterization state of the D3D12 pipeline.");
struct Rasterizer
{
  DOCUMENT("The mask determining which samples are written to.");
  uint32_t SampleMask = ~0U;

  DOCUMENT("A list of :class:`D3D12_Viewport` with the bound viewports.");
  rdctype::array<Viewport> Viewports;

  DOCUMENT("A list of :class:`D3D12_Scissor` with the bound scissor regions.");
  rdctype::array<Scissor> Scissors;

  DOCUMENT("A :class:`D3D12_RasterizerState` with the details of the rasterization state.");
  RasterizerState m_State;
};

DOCUMENT("Describes the details of a D3D12 stencil operation.");
struct StencilFace
{
  DOCUMENT("The :class:`StencilOp` to apply if the stencil-test fails.");
  StencilOp FailOp = StencilOp::Keep;
  DOCUMENT("The :class:`StencilOp` to apply if the depth-test fails.");
  StencilOp DepthFailOp = StencilOp::Keep;
  DOCUMENT("The :class:`StencilOp` to apply if the stencil-test passes.");
  StencilOp PassOp = StencilOp::Keep;
  DOCUMENT("The :class:`CompareFunc` to use for testing stencil values.");
  CompareFunc Func = CompareFunc::AlwaysTrue;
};

DOCUMENT("Describes the state of the depth-stencil state in the PSO.");
struct DepthStencilState
{
  DOCUMENT("``True`` if depth testing should be performed.");
  bool32 DepthEnable = false;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool32 DepthWrites = false;
  DOCUMENT("The :class:`CompareFunc` to use for testing depth values.");
  CompareFunc DepthFunc = CompareFunc::AlwaysTrue;
  DOCUMENT("``True`` if stencil operations should be performed.");
  bool32 StencilEnable = false;
  DOCUMENT("The mask for reading stencil values.");
  byte StencilReadMask = 0;
  DOCUMENT("The mask for writing stencil values.");
  byte StencilWriteMask = 0;

  DOCUMENT("A :class:`D3D12_StencilFace` describing what happens for front-facing polygons.");
  StencilFace m_FrontFace;
  DOCUMENT("A :class:`D3D12_StencilFace` describing what happens for back-facing polygons.");
  StencilFace m_BackFace;

  DOCUMENT("The current stencil reference value.");
  uint32_t StencilRef = 0;
};

DOCUMENT("Describes the details of a D3D12 blend operation.");
struct BlendEquation
{
  DOCUMENT("The :class:`BlendMultiplier` for the source blend value.");
  BlendMultiplier Source = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendMultiplier` for the destination blend value.");
  BlendMultiplier Destination = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendOp` to use in the blend calculation.");
  BlendOp Operation = BlendOp::Add;
};

DOCUMENT("Describes the blend configuration for a given D3D12 target.");
struct Blend
{
  DOCUMENT("A :class:`D3D12_BlendEquation` describing the blending for colour values.");
  BlendEquation m_Blend;
  DOCUMENT("A :class:`D3D12_BlendEquation` describing the blending for alpha values.");
  BlendEquation m_AlphaBlend;

  DOCUMENT(
      "The :class:`LogicOp` to use for logic operations, if :data:`LogicEnabled` is ``True``.");
  LogicOp Logic = LogicOp::NoOp;

  DOCUMENT("``True`` if blending is enabled for this target.");
  bool32 Enabled = false;
  DOCUMENT("``True`` if the logic operation in :data:`Logic` should be used.");
  bool32 LogicEnabled = false;
  DOCUMENT("The mask for writes to the render target.");
  byte WriteMask = 0;
};

DOCUMENT("Describes the blend state in the PSO.");
struct BlendState
{
  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool32 AlphaToCoverage = false;
  DOCUMENT(R"(``True`` if independent blending for each target should be used.

``False`` if the first blend should be applied to all targets.
)");
  bool32 IndependentBlend = false;

  DOCUMENT("A list of :class:`D3D12_Blend` describing the blend operations for each target.");
  rdctype::array<Blend> Blends;

  DOCUMENT("The constant blend factor to use in blend equations.");
  float BlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the current state of the output-merger stage of the D3D12 pipeline.");
struct OM
{
  DOCUMENT("A :class:`D3D12_DepthStencilState` with the details of the depth-stencil state.");
  DepthStencilState m_State;
  DOCUMENT("A :class:`D3D12_BlendState` with the details of the blend state.");
  BlendState m_BlendState;

  DOCUMENT("A list of :class:`D3D12_View` describing the bound render targets.");
  rdctype::array<View> RenderTargets;

  DOCUMENT("A :class:`D3D12_View` with details of the bound depth-stencil target.");
  View DepthTarget;
  DOCUMENT("``True`` if depth access to the depth-stencil target is read-only.");
  bool32 DepthReadOnly = false;
  DOCUMENT("``True`` if stenncil access to the depth-stencil target is read-only.");
  bool32 StencilReadOnly = false;

  DOCUMENT("The sample count used for rendering.");
  uint32_t multiSampleCount = 1;
  DOCUMENT("The MSAA quality level used for rendering.");
  uint32_t multiSampleQuality = 0;
};

DOCUMENT("Describes the current state that a sub-resource is in.");
struct ResourceState
{
  DOCUMENT("A human-readable name for the current state.");
  rdctype::str name;
};

DOCUMENT("Contains the current state of a given resource.");
struct ResourceData
{
  DOCUMENT("The :class:`ResourceId` of the resource.");
  ResourceId id;

  DOCUMENT("A list of :class:`D3D12_ResourceState` entries, one for each subresource.");
  rdctype::array<ResourceState> states;
};

DOCUMENT("The full current D3D12 pipeline state.");
struct State
{
  DOCUMENT("The :class:`ResourceId` of the pipeline state object.");
  ResourceId pipeline;

  DOCUMENT(R"(``True`` if :data:`name` was assigned by the application, otherwise it's autogenerated
based on the ID.
)");
  bool32 customName = false;

  DOCUMENT("The name of the pipeline state object.");
  rdctype::str name;

  DOCUMENT("The :class:`ResourceId` of the root signature object.");
  ResourceId rootSig;

  DOCUMENT("A :class:`D3D12_IA` describing the input assembly pipeline stage.");
  IA m_IA;

  DOCUMENT("A :class:`D3D12_Shader` describing the vertex shader stage.");
  Shader m_VS;
  DOCUMENT("A :class:`D3D12_Shader` describing the hull shader stage.");
  Shader m_HS;
  DOCUMENT("A :class:`D3D12_Shader` describing the domain shader stage.");
  Shader m_DS;
  DOCUMENT("A :class:`D3D12_Shader` describing the geometry shader stage.");
  Shader m_GS;
  DOCUMENT("A :class:`D3D12_Shader` describing the pixel shader stage.");
  Shader m_PS;
  DOCUMENT("A :class:`D3D12_Shader` describing the compute shader stage.");
  Shader m_CS;

  DOCUMENT("A :class:`D3D12_SO` describing the stream-out pipeline stage.");
  Streamout m_SO;

  DOCUMENT("A :class:`D3D12_Rasterizer` describing the rasterizer pipeline stage.");
  Rasterizer m_RS;

  DOCUMENT("A :class:`D3D12_OM` describing the output merger pipeline stage.");
  OM m_OM;

  DOCUMENT("A list of :class:`D3D12_ResourceData` entries, one for each resource.");
  rdctype::array<ResourceData> Resources;
};

};    // namespace D3D12Pipe

DECLARE_REFLECTION_STRUCT(D3D12Pipe::Layout);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::VB);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::IB);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::IA);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::View);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Sampler);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::CBuffer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RegisterSpace);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Shader);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::SOBind);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Streamout);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Viewport);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Scissor);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StencilFace);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::DepthStencilState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::BlendEquation);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Blend);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::OM);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceData);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::State);
