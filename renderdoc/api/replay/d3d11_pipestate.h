/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "shader_types.h"

namespace D3D11Pipe
{
DOCUMENT(R"(Describes a single D3D11 input layout element for one vertex input.

.. data:: TightlyPacked

  Value for :data:`ByteOffset` that indicates this element is tightly packed.
)");
struct Layout
{
  DOCUMENT("");
  bool operator==(const Layout &o) const
  {
    return SemanticName == o.SemanticName && SemanticIndex == o.SemanticIndex &&
           Format == o.Format && InputSlot == o.InputSlot && ByteOffset == o.ByteOffset &&
           PerInstance == o.PerInstance && InstanceDataStepRate == o.InstanceDataStepRate;
  }
  bool operator<(const Layout &o) const
  {
    if(!(SemanticName == o.SemanticName))
      return SemanticName < o.SemanticName;
    if(!(SemanticIndex == o.SemanticIndex))
      return SemanticIndex < o.SemanticIndex;
    if(!(Format == o.Format))
      return Format < o.Format;
    if(!(InputSlot == o.InputSlot))
      return InputSlot < o.InputSlot;
    if(!(ByteOffset == o.ByteOffset))
      return ByteOffset < o.ByteOffset;
    if(!(PerInstance == o.PerInstance))
      return PerInstance < o.PerInstance;
    if(!(InstanceDataStepRate == o.InstanceDataStepRate))
      return InstanceDataStepRate < o.InstanceDataStepRate;
    return false;
  }
  DOCUMENT("The semantic name for this input.");
  rdcstr SemanticName;

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
  bool PerInstance = false;

  DOCUMENT(R"(If :data:`PerInstance` is ``True`` then this is how many times each instance data is
used before advancing to the next instance.

E.g. if this value is two, then two instances will be drawn with the first instance data, then two
with the next instance data.
)");
  uint32_t InstanceDataStepRate = 0;

  // D3D11_APPEND_ALIGNED_ELEMENT
  static const uint32_t TightlyPacked = ~0U;
};

DOCUMENT("Describes a single D3D11 vertex buffer binding.")
struct VB
{
  DOCUMENT("");
  bool operator==(const VB &o) const
  {
    return Buffer == o.Buffer && Stride == o.Stride && Offset == o.Offset;
  }
  bool operator<(const VB &o) const
  {
    if(!(Buffer == o.Buffer))
      return Buffer < o.Buffer;
    if(!(Stride == o.Stride))
      return Stride < o.Stride;
    if(!(Offset == o.Offset))
      return Offset < o.Offset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId Buffer;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t Stride = 0;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint32_t Offset = 0;
};

DOCUMENT("Describes the D3D11 index buffer binding.")
struct IB
{
  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId Buffer;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint32_t Offset = 0;
};

DOCUMENT("Describes the input assembler data.");
struct IA
{
  DOCUMENT("A list of :class:`D3D11_Layout` describing the input layout elements in this layout.");
  rdcarray<Layout> layouts;

  DOCUMENT("The :class:`ResourceId` of the layout object.");
  ResourceId layout;

  DOCUMENT("A :class:`ShaderReflection` describing the bytecode used to create the input layout.");
  ShaderReflection *Bytecode = NULL;

  DOCUMENT("A list of :class:`D3D11_VB` with the vertex buffers that are bound.");
  rdcarray<VB> vbuffers;

  DOCUMENT("The :class:`D3D11_IB` describing the index buffer.");
  IB ibuffer;
};

DOCUMENT("Describes the details of a D3D11 resource view - any one of UAV, SRV, RTV or DSV.");
struct View
{
  DOCUMENT("");
  bool operator==(const View &o) const
  {
    return Object == o.Object && Resource == o.Resource && Type == o.Type && Format == o.Format &&
           Structured == o.Structured && BufferStructCount == o.BufferStructCount &&
           ElementSize == o.ElementSize && FirstElement == o.FirstElement &&
           NumElements == o.NumElements && Flags == o.Flags && HighestMip == o.HighestMip &&
           NumMipLevels == o.NumMipLevels && ArraySize == o.ArraySize &&
           FirstArraySlice == o.FirstArraySlice;
  }
  bool operator<(const View &o) const
  {
    if(!(Object == o.Object))
      return Object < o.Object;
    if(!(Resource == o.Resource))
      return Resource < o.Resource;
    if(!(Type == o.Type))
      return Type < o.Type;
    if(!(Format == o.Format))
      return Format < o.Format;
    if(!(Structured == o.Structured))
      return Structured < o.Structured;
    if(!(BufferStructCount == o.BufferStructCount))
      return BufferStructCount < o.BufferStructCount;
    if(!(ElementSize == o.ElementSize))
      return ElementSize < o.ElementSize;
    if(!(FirstElement == o.FirstElement))
      return FirstElement < o.FirstElement;
    if(!(NumElements == o.NumElements))
      return NumElements < o.NumElements;
    if(!(Flags == o.Flags))
      return Flags < o.Flags;
    if(!(HighestMip == o.HighestMip))
      return HighestMip < o.HighestMip;
    if(!(NumMipLevels == o.NumMipLevels))
      return NumMipLevels < o.NumMipLevels;
    if(!(ArraySize == o.ArraySize))
      return ArraySize < o.ArraySize;
    if(!(FirstArraySlice == o.FirstArraySlice))
      return FirstArraySlice < o.FirstArraySlice;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the view itself.");
  ResourceId Object;

  DOCUMENT("The :class:`ResourceId` of the underlying resource the view refers to.");
  ResourceId Resource;

  DOCUMENT("The :class:`TextureDim` of the view type.");
  TextureDim Type;

  DOCUMENT("The :class:`ResourceFormat` that the view uses.");
  ResourceFormat Format;

  DOCUMENT("``True`` if this view describes a structured buffer.");
  bool Structured = false;

  DOCUMENT("If the view has a hidden counter, this stores the current value of the counter.");
  uint32_t BufferStructCount = 0;

  DOCUMENT(R"(The byte size of a single element in the view. Either the byte size of :data:`Format`,
or the structured buffer element size, as appropriate.
)");
  uint32_t ElementSize = 0;

  DOCUMENT("Valid for buffers - the first element to be used in the view.");
  uint32_t FirstElement = 0;
  DOCUMENT("Valid for buffers - the number of elements to be used in the view.");
  uint32_t NumElements = 1;

  DOCUMENT("Valid for buffers - the flags for additional view properties.");
  D3DBufferViewFlags Flags = D3DBufferViewFlags::NoFlags;

  DOCUMENT("Valid for textures - the highest mip that is available through the view.");
  uint32_t HighestMip = 0;
  DOCUMENT("Valid for textures - the number of mip levels in the view.");
  uint32_t NumMipLevels = 0;

  DOCUMENT("Valid for texture arrays or 3D textures - the number of slices in the view.");
  uint32_t ArraySize = 1;
  DOCUMENT("Valid for texture arrays or 3D textures - the first slice available through the view.");
  uint32_t FirstArraySlice = 0;
};

DOCUMENT("Describes a sampler state object.");
struct Sampler
{
  DOCUMENT("");
  bool operator==(const Sampler &o) const
  {
    return Samp == o.Samp && AddressU == o.AddressU && AddressV == o.AddressV &&
           AddressW == o.AddressW && BorderColor[0] == o.BorderColor[0] &&
           BorderColor[1] == o.BorderColor[1] && BorderColor[2] == o.BorderColor[2] &&
           BorderColor[3] == o.BorderColor[3] && Comparison == o.Comparison && Filter == o.Filter &&
           MaxAniso == o.MaxAniso && MaxLOD == o.MaxLOD && MinLOD == o.MinLOD &&
           MipLODBias == o.MipLODBias;
  }
  bool operator<(const Sampler &o) const
  {
    if(!(Samp == o.Samp))
      return Samp < o.Samp;
    if(!(AddressU == o.AddressU))
      return AddressU < o.AddressU;
    if(!(AddressV == o.AddressV))
      return AddressV < o.AddressV;
    if(!(AddressW == o.AddressW))
      return AddressW < o.AddressW;
    if(!(BorderColor[0] == o.BorderColor[0]))
      return BorderColor[0] < o.BorderColor[0];
    if(!(BorderColor[1] == o.BorderColor[1]))
      return BorderColor[1] < o.BorderColor[1];
    if(!(BorderColor[2] == o.BorderColor[2]))
      return BorderColor[2] < o.BorderColor[2];
    if(!(BorderColor[3] == o.BorderColor[3]))
      return BorderColor[3] < o.BorderColor[3];
    if(!(Comparison == o.Comparison))
      return Comparison < o.Comparison;
    if(!(Filter == o.Filter))
      return Filter < o.Filter;
    if(!(MaxAniso == o.MaxAniso))
      return MaxAniso < o.MaxAniso;
    if(!(MaxLOD == o.MaxLOD))
      return MaxLOD < o.MaxLOD;
    if(!(MinLOD == o.MinLOD))
      return MinLOD < o.MinLOD;
    if(!(MipLODBias == o.MipLODBias))
      return MipLODBias < o.MipLODBias;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the sampler state object.");
  ResourceId Samp;
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

  DOCUMENT(R"(Check if the border color is used in this D3D11 sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: ``bool``
)");
  bool UseBorder() const
  {
    return AddressU == AddressMode::ClampBorder || AddressV == AddressMode::ClampBorder ||
           AddressW == AddressMode::ClampBorder;
  }
};

DOCUMENT("Describes a constant buffer binding.");
struct CBuffer
{
  DOCUMENT("");
  bool operator==(const CBuffer &o) const
  {
    return Buffer == o.Buffer && VecOffset == o.VecOffset && VecCount == o.VecCount;
  }
  bool operator<(const CBuffer &o) const
  {
    if(!(Buffer == o.Buffer))
      return Buffer < o.Buffer;
    if(!(VecOffset == o.VecOffset))
      return VecOffset < o.VecOffset;
    if(!(VecCount == o.VecCount))
      return VecCount < o.VecCount;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer.");
  ResourceId Buffer;

  DOCUMENT(R"(The offset of the buffer binding, in units of ``float4`` (16 bytes).

If the capture isn't using the D3D11.1 binding methods, this offset will be 0.
)");
  uint32_t VecOffset = 0;

  DOCUMENT(R"(The size of the buffer binding, in units of ``float4`` (16 bytes).

If the capture isn't using the D3D11.1 binding methods, this offset will be 4096 (64 kiB).
)");
  uint32_t VecCount = 0;
};

DOCUMENT("Describes a D3D11 shader stage.");
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

  DOCUMENT("A list of :class:`D3D11_View` with the bound SRVs.");
  rdcarray<View> SRVs;

  DOCUMENT("A list of :class:`D3D11_View` with the bound UAVs - only valid for the compute stage.");
  rdcarray<View> UAVs;

  DOCUMENT("A list of :class:`D3D11_Sampler` with the bound samplers.");
  rdcarray<Sampler> Samplers;

  DOCUMENT("A list of :class:`D3D11_CBuffer` with the bound constant buffers.");
  rdcarray<CBuffer> ConstantBuffers;

  DOCUMENT("A list of ``str`` with the bound class instance names.");
  rdcarray<rdcstr> ClassInstances;
};

DOCUMENT("Describes a binding on the D3D11 stream-out stage.");
struct SOBind
{
  DOCUMENT("");
  bool operator==(const SOBind &o) const { return Buffer == o.Buffer && Offset == o.Offset; }
  bool operator<(const SOBind &o) const
  {
    if(!(Buffer == o.Buffer))
      return Buffer < o.Buffer;
    if(!(Offset == o.Offset))
      return Offset < o.Offset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer.");
  ResourceId Buffer;

  DOCUMENT("The byte offset of the stream-output binding.");
  uint32_t Offset = 0;
};

DOCUMENT("Describes the stream-out stage bindings.");
struct SO
{
  DOCUMENT("A list of ``D3D11_SOBind`` with the bound buffers.");
  rdcarray<SOBind> Outputs;
};

DOCUMENT("Describes a single D3D11 viewport.");
struct Viewport
{
  DOCUMENT("");
  bool operator==(const Viewport &o) const
  {
    return X == o.X && Y == o.Y && Width == o.Width && Height == o.Height &&
           MinDepth == o.MinDepth && MaxDepth == o.MaxDepth;
  }
  bool operator<(const Viewport &o) const
  {
    if(!(X == o.X))
      return X < o.X;
    if(!(Y == o.Y))
      return Y < o.Y;
    if(!(Width == o.Width))
      return Width < o.Width;
    if(!(Height == o.Height))
      return Height < o.Height;
    if(!(MinDepth == o.MinDepth))
      return MinDepth < o.MinDepth;
    if(!(MaxDepth == o.MaxDepth))
      return MaxDepth < o.MaxDepth;
    return false;
  }
  Viewport() = default;
  Viewport(float TX, float TY, float W, float H, float MN, float MX, bool en)
      : X(TX), Y(TY), Width(W), Height(H), MinDepth(MN), MaxDepth(MX), Enabled(en)
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
  DOCUMENT("``True`` if this viewport is enabled.");
  bool Enabled = false;
};

DOCUMENT("Describes a single D3D11 scissor rect.");
struct Scissor
{
  DOCUMENT("");
  bool operator==(const Scissor &o) const
  {
    return left == o.left && top == o.top && right == o.right && bottom == o.bottom &&
           Enabled == o.Enabled;
  }
  bool operator<(const Scissor &o) const
  {
    if(!(left == o.left))
      return left < o.left;
    if(!(top == o.top))
      return top < o.top;
    if(!(right == o.right))
      return right < o.right;
    if(!(bottom == o.bottom))
      return bottom < o.bottom;
    if(!(Enabled == o.Enabled))
      return Enabled < o.Enabled;
    return false;
  }
  Scissor() = default;
  Scissor(int l, int t, int r, int b, bool en) : left(l), top(t), right(r), bottom(b), Enabled(en)
  {
  }
  DOCUMENT("Top-left X co-ordinate of the viewport.");
  int32_t left = 0;
  DOCUMENT("Top-left Y co-ordinate of the viewport.");
  int32_t top = 0;
  DOCUMENT("Bottom-right X co-ordinate of the viewport.");
  int32_t right = 0;
  DOCUMENT("Bottom-right Y co-ordinate of the viewport.");
  int32_t bottom = 0;
  DOCUMENT("``True`` if this scissor region is enabled.");
  bool Enabled = false;
};

DOCUMENT("Describes a rasterizer state object.");
struct RasterizerState
{
  DOCUMENT("The :class:`ResourceId` of the rasterizer state object.");
  ResourceId State;
  DOCUMENT("The polygon fill mode.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon culling mode.");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool FrontCCW = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  int32_t DepthBias = 0;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`DepthBias` and
:data:`SlopeScaledDepthBias`
)");
  float DepthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float SlopeScaledDepthBias = 0.0f;
  DOCUMENT("``True`` if pixels outside of the near and far depth planes should be clipped.");
  bool DepthClip = false;
  DOCUMENT("``True`` if the scissor test should be applied.");
  bool ScissorEnable = false;
  DOCUMENT("``True`` if the quadrilateral MSAA algorithm should be used on MSAA targets.");
  bool MultisampleEnable = false;
  DOCUMENT(
      "``True`` if lines should be anti-aliased. Ignored if :data:`MultisampleEnable` is "
      "``False``.");
  bool AntialiasedLineEnable = false;
  DOCUMENT(R"(A sample count to force rasterization to when UAV rendering or rasterizing, or 0 to
not force any sample count.
)");
  uint32_t ForcedSampleCount = 0;
  DOCUMENT("``True`` if a conservative rasterization algorithm should be used.");
  bool ConservativeRasterization = false;
};

DOCUMENT("Describes the rasterization state of the D3D11 pipeline.");
struct Rasterizer
{
  DOCUMENT("A list of :class:`D3D11_Viewport` with the bound viewports.");
  rdcarray<Viewport> Viewports;

  DOCUMENT("A list of :class:`D3D11_Scissor` with the bound scissor regions.");
  rdcarray<Scissor> Scissors;

  DOCUMENT("A :class:`D3D11_RasterizerState` with the details of the rasterization state.");
  RasterizerState m_State;
};

DOCUMENT("Describes the details of a D3D11 stencil operation.");
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

DOCUMENT("Describes a depth-stencil state object.");
struct DepthStencilState
{
  DOCUMENT("The :class:`ResourceId` of the depth-stencil state object.");
  ResourceId State;
  DOCUMENT("``True`` if depth testing should be performed.");
  bool DepthEnable = false;
  DOCUMENT("The :class:`CompareFunc` to use for testing depth values.");
  CompareFunc DepthFunc = CompareFunc::AlwaysTrue;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool DepthWrites = false;
  DOCUMENT("``True`` if stencil operations should be performed.");
  bool StencilEnable = false;
  DOCUMENT("The mask for reading stencil values.");
  byte StencilReadMask = 0;
  DOCUMENT("The mask for writing stencil values.");
  byte StencilWriteMask = 0;

  DOCUMENT("A :class:`D3D11_StencilFace` describing what happens for front-facing polygons.");
  StencilFace m_FrontFace;
  DOCUMENT("A :class:`D3D11_StencilFace` describing what happens for back-facing polygons.");
  StencilFace m_BackFace;

  DOCUMENT("The current stencil reference value.");
  uint32_t StencilRef = 0;
};

DOCUMENT("Describes the details of a D3D11 blend operation.");
struct BlendEquation
{
  DOCUMENT("");
  bool operator==(const BlendEquation &o) const
  {
    return Source == o.Source && Destination == o.Destination && Operation == o.Operation;
  }
  bool operator<(const BlendEquation &o) const
  {
    if(!(Source == o.Source))
      return Source < o.Source;
    if(!(Destination == o.Destination))
      return Destination < o.Destination;
    if(!(Operation == o.Operation))
      return Operation < o.Operation;
    return false;
  }
  DOCUMENT("The :class:`BlendMultiplier` for the source blend value.");
  BlendMultiplier Source = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendMultiplier` for the destination blend value.");
  BlendMultiplier Destination = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendOp` to use in the blend calculation.");
  BlendOp Operation = BlendOp::Add;
};

DOCUMENT("Describes the blend configuration for a given D3D11 target.");
struct Blend
{
  DOCUMENT("");
  bool operator==(const Blend &o) const
  {
    return Enabled == o.Enabled && LogicEnabled == o.LogicEnabled && m_Blend == o.m_Blend &&
           m_AlphaBlend == o.m_AlphaBlend && Logic == o.Logic && WriteMask == o.WriteMask;
  }
  bool operator<(const Blend &o) const
  {
    if(!(Enabled == o.Enabled))
      return Enabled < o.Enabled;
    if(!(LogicEnabled == o.LogicEnabled))
      return LogicEnabled < o.LogicEnabled;
    if(!(m_Blend == o.m_Blend))
      return m_Blend < o.m_Blend;
    if(!(m_AlphaBlend == o.m_AlphaBlend))
      return m_AlphaBlend < o.m_AlphaBlend;
    if(!(Logic == o.Logic))
      return Logic < o.Logic;
    if(!(WriteMask == o.WriteMask))
      return WriteMask < o.WriteMask;
    return false;
  }

  DOCUMENT("A :class:`D3D11_BlendEquation` describing the blending for colour values.");
  BlendEquation m_Blend;
  DOCUMENT("A :class:`D3D11_BlendEquation` describing the blending for alpha values.");
  BlendEquation m_AlphaBlend;

  DOCUMENT(
      "The :class:`LogicOp` to use for logic operations, if :data:`LogicEnabled` is ``True``.");
  LogicOp Logic = LogicOp::NoOp;

  DOCUMENT("``True`` if blending is enabled for this target.");
  bool Enabled = false;
  DOCUMENT("``True`` if the logic operation in :data:`Logic` should be used.");
  bool LogicEnabled = false;
  DOCUMENT("The mask for writes to the render target.");
  byte WriteMask = 0;
};

DOCUMENT("Describes a blend state object.");
struct BlendState
{
  DOCUMENT("The :class:`ResourceId` of the blend state object.");
  ResourceId State;

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool AlphaToCoverage = false;
  DOCUMENT(R"(``True`` if independent blending for each target should be used.

``False`` if the first blend should be applied to all targets.
)");
  bool IndependentBlend = false;

  DOCUMENT("A list of :class:`D3D11_Blend` describing the blend operations for each target.");
  rdcarray<Blend> Blends;

  DOCUMENT("The constant blend factor to use in blend equations.");
  float BlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  DOCUMENT("The mask determining which samples are written to.");
  uint32_t SampleMask = ~0U;
};

DOCUMENT("Describes the current state of the output-merger stage of the D3D11 pipeline.");
struct OM
{
  DOCUMENT("A :class:`D3D11_DepthStencilState` with the details of the depth-stencil state.");
  DepthStencilState m_State;
  DOCUMENT("A :class:`D3D11_BlendState` with the details of the blend state.");
  BlendState m_BlendState;

  DOCUMENT("A list of :class:`D3D11_View` describing the bound render targets.");
  rdcarray<View> RenderTargets;

  DOCUMENT("Which slot in the output targets is the first UAV.");
  uint32_t UAVStartSlot = 0;
  DOCUMENT("A list of :class:`D3D11_View` describing the bound UAVs.");
  rdcarray<View> UAVs;

  DOCUMENT("A :class:`D3D11_View` with details of the bound depth-stencil target.");
  View DepthTarget;
  DOCUMENT("``True`` if depth access to the depth-stencil target is read-only.");
  bool DepthReadOnly = false;
  DOCUMENT("``True`` if stencil access to the depth-stencil target is read-only.");
  bool StencilReadOnly = false;
};

DOCUMENT("Describes the current state of predicated rendering.");
struct Predication
{
  DOCUMENT("The :class:`ResourceId` of the active predicate.");
  ResourceId Obj;

  DOCUMENT("The value to go along with the predicate.");
  bool Value = false;

  DOCUMENT("``True`` if the current predicate would render.");
  bool Passing = false;
};

DOCUMENT("The full current D3D11 pipeline state.");
struct State
{
  DOCUMENT("A :class:`D3D11_IA` describing the input assembly pipeline stage.");
  IA m_IA;

  DOCUMENT("A :class:`D3D11_Shader` describing the vertex shader stage.");
  Shader m_VS;
  DOCUMENT("A :class:`D3D11_Shader` describing the hull shader stage.");
  Shader m_HS;
  DOCUMENT("A :class:`D3D11_Shader` describing the domain shader stage.");
  Shader m_DS;
  DOCUMENT("A :class:`D3D11_Shader` describing the geometry shader stage.");
  Shader m_GS;
  DOCUMENT("A :class:`D3D11_Shader` describing the pixel shader stage.");
  Shader m_PS;
  DOCUMENT("A :class:`D3D11_Shader` describing the compute shader stage.");
  Shader m_CS;

  DOCUMENT("A :class:`D3D11_SO` describing the stream-out pipeline stage.");
  SO m_SO;

  DOCUMENT("A :class:`D3D11_Rasterizer` describing the rasterizer pipeline stage.");
  Rasterizer m_RS;

  DOCUMENT("A :class:`D3D11_OM` describing the output merger pipeline stage.");
  OM m_OM;

  DOCUMENT("A :class:`Predication` describing the predicated rendering state.");
  Predication m_Predicate;
};

};    // namespace D3D11Pipe

DECLARE_REFLECTION_STRUCT(D3D11Pipe::Layout);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::VB);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::IB);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::IA);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::View);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Sampler);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::CBuffer);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Shader);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::SOBind);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::SO);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Viewport);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Scissor);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::DepthStencilState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::StencilFace);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Blend);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::BlendEquation);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::OM);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Predication);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::State);
