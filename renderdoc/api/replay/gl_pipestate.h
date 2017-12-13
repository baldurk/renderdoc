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

#include "data_types.h"

namespace GLPipe
{
DOCUMENT(R"(Describes the configuration for a single vertex attribute.

.. note:: If old-style vertex attrib pointer setup was used for the vertex attributes then it will
  be decomposed into 1:1 attributes and buffers.
)");
struct VertexAttribute
{
  DOCUMENT("");
  bool operator==(const VertexAttribute &o) const
  {
    return Enabled == o.Enabled && Format == o.Format &&
           !memcmp(&GenericValue, &o.GenericValue, sizeof(GenericValue)) &&
           BufferSlot == o.BufferSlot && RelativeOffset == o.RelativeOffset;
  }
  bool operator<(const VertexAttribute &o) const
  {
    if(!(Enabled == o.Enabled))
      return Enabled < o.Enabled;
    if(!(Format == o.Format))
      return Format < o.Format;
    if(memcmp(&GenericValue, &o.GenericValue, sizeof(GenericValue)) < 0)
      return true;
    if(!(BufferSlot == o.BufferSlot))
      return BufferSlot < o.BufferSlot;
    if(!(RelativeOffset == o.RelativeOffset))
      return RelativeOffset < o.RelativeOffset;
    return false;
  }
  DOCUMENT("``True`` if this vertex attribute is enabled.");
  bool Enabled = false;
  DOCUMENT("The :class:`ResourceFormat` of the vertex attribute.");
  ResourceFormat Format;

  DOCUMENT("A :class:`PixelValue` containing the generic value of a vertex attribute.");
  PixelValue GenericValue;

  DOCUMENT("The vertex buffer input slot where the data is sourced from.");
  uint32_t BufferSlot = 0;
  DOCUMENT(R"(The byte offset from the start of the vertex data in the vertex buffer from
:data:`BufferSlot`.
)");
  uint32_t RelativeOffset = 0;
};

DOCUMENT("Describes a single OpenGL vertex buffer binding.")
struct VB
{
  DOCUMENT("");
  bool operator==(const VB &o) const
  {
    return Buffer == o.Buffer && Stride == o.Stride && Offset == o.Offset && Divisor == o.Divisor;
  }
  bool operator<(const VB &o) const
  {
    if(!(Buffer == o.Buffer))
      return Buffer < o.Buffer;
    if(!(Stride == o.Stride))
      return Stride < o.Stride;
    if(!(Offset == o.Offset))
      return Offset < o.Offset;
    if(!(Divisor == o.Divisor))
      return Divisor < o.Divisor;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId Buffer;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t Stride = 0;
  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint32_t Offset = 0;
  DOCUMENT(R"(The instance rate divisor.

If this is ``0`` then the vertex buffer is read at vertex rate.

If it's ``1`` then one element is read for each instance, and for ``N`` greater than ``1`` then
``N`` instances read the same element before advancing.
)");
  uint32_t Divisor = 0;
};

DOCUMENT("Describes the setup for fixed-function vertex input fetch.");
struct VertexInput
{
  DOCUMENT("A list of :class:`GL_VertexAttribute` with the vertex attributes.");
  rdcarray<VertexAttribute> attributes;

  DOCUMENT("A list of :class:`GL_VB` with the vertex buffers.");
  rdcarray<VB> vbuffers;

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId ibuffer;
  DOCUMENT("``True`` if primitive restart is enabled for strip primitives.");
  bool primitiveRestart = false;
  DOCUMENT("The index value to use to indicate a strip restart.");
  uint32_t restartIndex = 0;

  DOCUMENT(R"(``True`` if the provoking vertex is the last one in the primitive.

``False`` if the provoking vertex is the first one.
)");
  bool provokingVertexLast = false;
};

DOCUMENT("Describes an OpenGL shader stage.");
struct Shader
{
  DOCUMENT("The :class:`ResourceId` of the shader object itself.");
  ResourceId Object;

  DOCUMENT("The :class:`ResourceId` of the program bound to this stage.");
  ResourceId Program;

  DOCUMENT("A :class:`ShaderReflection` describing the reflection data for this shader.");
  ShaderReflection *ShaderDetails = NULL;
  DOCUMENT(R"(A :class:`ShaderBindpointMapping` to match :data:`ShaderDetails` with the bindpoint
mapping data.
)");
  ShaderBindpointMapping BindpointMapping;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT("A list of integers with the subroutine values.");
  rdcarray<uint32_t> Subroutines;
};

DOCUMENT("Describes the setup for fixed vertex processing operations.");
struct FixedVertexProcessing
{
  DOCUMENT("A list of ``float`` giving the default inner level of tessellation.");
  float defaultInnerLevel[2] = {0.0f, 0.0f};
  DOCUMENT("A list of ``float`` giving the default outer level of tessellation.");
  float defaultOuterLevel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT("``True`` if primitives should be discarded during rasterization.");
  bool discard = false;

  DOCUMENT("A list of ``bool`` determining which user clipping planes are enabled.");
  bool clipPlanes[8] = {false, false, false, false, false, false, false, false};
  DOCUMENT(R"(``True`` if the clipping origin should be in the lower left.

``False`` if it's in the upper left.
)");
  bool clipOriginLowerLeft = false;
  DOCUMENT(R"(``True`` if the clip-space Z goes from ``-1`` to ``1``.

``False`` if the clip-space Z goes from ``0`` to ``1``.
)");
  bool clipNegativeOneToOne = false;
};

DOCUMENT("Describes the details of a texture.");
struct Texture
{
  DOCUMENT("");
  bool operator==(const Texture &o) const
  {
    return Resource == o.Resource && FirstSlice == o.FirstSlice && HighestMip == o.HighestMip &&
           ResType == o.ResType && Swizzle[0] == o.Swizzle[0] && Swizzle[1] == o.Swizzle[1] &&
           Swizzle[2] == o.Swizzle[2] && Swizzle[3] == o.Swizzle[3] &&
           DepthReadChannel == o.DepthReadChannel;
  }
  bool operator<(const Texture &o) const
  {
    if(!(Resource == o.Resource))
      return Resource < o.Resource;
    if(!(FirstSlice == o.FirstSlice))
      return FirstSlice < o.FirstSlice;
    if(!(HighestMip == o.HighestMip))
      return HighestMip < o.HighestMip;
    if(!(ResType == o.ResType))
      return ResType < o.ResType;
    if(!(Swizzle[0] == o.Swizzle[0]))
      return Swizzle[0] < o.Swizzle[0];
    if(!(Swizzle[1] == o.Swizzle[1]))
      return Swizzle[1] < o.Swizzle[1];
    if(!(Swizzle[2] == o.Swizzle[2]))
      return Swizzle[2] < o.Swizzle[2];
    if(!(Swizzle[3] == o.Swizzle[3]))
      return Swizzle[3] < o.Swizzle[3];
    if(!(DepthReadChannel == o.DepthReadChannel))
      return DepthReadChannel < o.DepthReadChannel;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the underlying resource the view refers to.");
  ResourceId Resource;
  DOCUMENT("Valid for texture arrays or 3D textures - the first slice available.");
  uint32_t FirstSlice = 0;
  DOCUMENT("Valid for textures - the highest mip that is available.");
  uint32_t HighestMip = 0;
  DOCUMENT("The :class:`TextureDim` of the texture.");
  TextureDim ResType = TextureDim::Unknown;

  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle Swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  DOCUMENT(R"(The channel to read from in a depth-stencil texture.

``-1`` for non depth-stencil textures.

``0`` if depth should be read.

``1`` if stencil should be read.
)");
  int32_t DepthReadChannel = -1;
};

DOCUMENT("Describes the sampler properties of a texture.");
struct Sampler
{
  DOCUMENT("");
  bool operator==(const Sampler &o) const
  {
    return Samp == o.Samp && AddressS == o.AddressS && AddressT == o.AddressT &&
           AddressR == o.AddressR && BorderColor[0] == o.BorderColor[0] &&
           BorderColor[1] == o.BorderColor[1] && BorderColor[2] == o.BorderColor[2] &&
           BorderColor[3] == o.BorderColor[3] && Comparison == o.Comparison && Filter == o.Filter &&
           SeamlessCube == o.SeamlessCube && MaxAniso == o.MaxAniso && MaxLOD == o.MaxLOD &&
           MinLOD == o.MinLOD && MipLODBias == o.MipLODBias;
  }
  bool operator<(const Sampler &o) const
  {
    if(!(Samp == o.Samp))
      return Samp < o.Samp;
    if(!(AddressS == o.AddressS))
      return AddressS < o.AddressS;
    if(!(AddressT == o.AddressT))
      return AddressT < o.AddressT;
    if(!(AddressR == o.AddressR))
      return AddressR < o.AddressR;
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
    if(!(SeamlessCube == o.SeamlessCube))
      return SeamlessCube < o.SeamlessCube;
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
  DOCUMENT("The :class:`ResourceId` of the sampler object, if a separate one is set.");
  ResourceId Samp;
  DOCUMENT("The :class:`AddressMode` in the S direction.");
  AddressMode AddressS = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the T direction.");
  AddressMode AddressT = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the R direction.");
  AddressMode AddressR = AddressMode::Wrap;
  DOCUMENT("The RGBA border color.");
  float BorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT("The :class:`CompareFunc` for comparison samplers.");
  CompareFunc Comparison = CompareFunc::AlwaysTrue;
  DOCUMENT("The :class:`TextureFilter` describing the filtering mode.");
  TextureFilter Filter;
  DOCUMENT("``True`` if seamless cubemap filtering is enabled for this texture.");
  bool SeamlessCube = false;
  DOCUMENT("The maximum anisotropic filtering level to use.");
  float MaxAniso = 0.0f;
  DOCUMENT("The maximum mip level that can be used.");
  float MaxLOD = 0.0f;
  DOCUMENT("The minimum mip level that can be used.");
  float MinLOD = 0.0f;
  DOCUMENT("A bias to apply to the calculated mip level before sampling.");
  float MipLODBias = 0.0f;

  DOCUMENT(R"(Check if the border color is used in this OpenGL sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: bool
)");
  bool UseBorder() const
  {
    return AddressS == AddressMode::ClampBorder || AddressT == AddressMode::ClampBorder ||
           AddressR == AddressMode::ClampBorder;
  }
};

DOCUMENT("Describes the properties of a buffer.");
struct Buffer
{
  DOCUMENT("");
  bool operator==(const Buffer &o) const
  {
    return Resource == o.Resource && Offset == o.Offset && Size == o.Size;
  }
  bool operator<(const Buffer &o) const
  {
    if(!(Resource == o.Resource))
      return Resource < o.Resource;
    if(!(Offset == o.Offset))
      return Offset < o.Offset;
    if(!(Size == o.Size))
      return Size < o.Size;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer object.");
  ResourceId Resource;
  DOCUMENT("The byte offset from the start of the buffer.");
  uint64_t Offset = 0;
  DOCUMENT("The byte size of the buffer.");
  uint64_t Size = 0;
};

DOCUMENT("Describes the properties of a load/store image.");
struct ImageLoadStore
{
  DOCUMENT("");
  bool operator==(const ImageLoadStore &o) const
  {
    return Resource == o.Resource && Level == o.Level && Layered == o.Layered && Layer == o.Layer &&
           ResType == o.ResType && readAllowed == o.readAllowed && writeAllowed == o.writeAllowed &&
           Format == o.Format;
  }
  bool operator<(const ImageLoadStore &o) const
  {
    if(!(Resource == o.Resource))
      return Resource < o.Resource;
    if(!(Level == o.Level))
      return Level < o.Level;
    if(!(Layered == o.Layered))
      return Layered < o.Layered;
    if(!(Layer == o.Layer))
      return Layer < o.Layer;
    if(!(ResType == o.ResType))
      return ResType < o.ResType;
    if(!(readAllowed == o.readAllowed))
      return readAllowed < o.readAllowed;
    if(!(writeAllowed == o.writeAllowed))
      return writeAllowed < o.writeAllowed;
    if(!(Format == o.Format))
      return Format < o.Format;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the texture object.");
  ResourceId Resource;
  DOCUMENT("The mip of the texture that's used in the attachment.");
  uint32_t Level = 0;
  DOCUMENT(R"(``True`` if multiple layers are bound together to the image.
``False`` if only one layer is bound.
)");
  bool Layered = false;
  DOCUMENT("The slice of the texture that's used in the attachment.");
  uint32_t Layer = 0;
  DOCUMENT("The :class:`TextureDim` of the texture.");
  TextureDim ResType = TextureDim::Unknown;
  DOCUMENT("``True`` if loading from the image is allowed.");
  bool readAllowed = false;
  DOCUMENT("``True`` if storing to the image is allowed.");
  bool writeAllowed = false;
  DOCUMENT("The :class:`ResourceFormat` that the image is bound as.");
  ResourceFormat Format;
};

DOCUMENT("Describes the current feedback state.");
struct Feedback
{
  DOCUMENT("The :class:`ResourceId` of the transform feedback binding.");
  ResourceId Obj;
  DOCUMENT("A list of :class:`ResourceId` with the buffer bindings.");
  ResourceId BufferBinding[4];
  DOCUMENT("A list of ``int`` with the buffer byte offsets.");
  uint64_t Offset[4] = {0, 0, 0, 0};
  DOCUMENT("A list of ``int`` with the buffer byte sizes.");
  uint64_t Size[4] = {0, 0, 0, 0};
  DOCUMENT("``True`` if the transform feedback object is currently active.");
  bool Active = false;
  DOCUMENT("``True`` if the transform feedback object is currently paused.");
  bool Paused = false;
};

DOCUMENT("Describes a single OpenGL viewport.");
struct Viewport
{
  DOCUMENT("");
  bool operator==(const Viewport &o) const
  {
    return Left == o.Left && Bottom == o.Bottom && Width == o.Width && Height == o.Height &&
           MinDepth == o.MinDepth && MaxDepth == o.MaxDepth;
  }
  bool operator<(const Viewport &o) const
  {
    if(!(Left == o.Left))
      return Left < o.Left;
    if(!(Bottom == o.Bottom))
      return Bottom < o.Bottom;
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
  DOCUMENT("The X co-ordinate of the left side of the viewport.");
  float Left = 0.0f;
  DOCUMENT("The Y co-ordinate of the bottom side of the viewport.");
  float Bottom = 0.0f;
  DOCUMENT("The width of the viewport.");
  float Width = 0.0f;
  DOCUMENT("The height of the viewport.");
  float Height = 0.0f;
  DOCUMENT("The minimum depth of the viewport.");
  double MinDepth = 0.0;
  DOCUMENT("The maximum depth of the viewport.");
  double MaxDepth = 0.0;
};

DOCUMENT("Describes a single OpenGL scissor region.");
struct Scissor
{
  DOCUMENT("");
  bool operator==(const Scissor &o) const
  {
    return Left == o.Left && Bottom == o.Bottom && Width == o.Width && Height == o.Height &&
           Enabled == o.Enabled;
  }
  bool operator<(const Scissor &o) const
  {
    if(!(Left == o.Left))
      return Left < o.Left;
    if(!(Bottom == o.Bottom))
      return Bottom < o.Bottom;
    if(!(Width == o.Width))
      return Width < o.Width;
    if(!(Height == o.Height))
      return Height < o.Height;
    if(!(Enabled == o.Enabled))
      return Enabled < o.Enabled;
    return false;
  }
  DOCUMENT("The X co-ordinate of the left side of the scissor region.");
  int32_t Left = 0;
  DOCUMENT("The Y co-ordinate of the bottom side of the scissor region.");
  int32_t Bottom = 0;
  DOCUMENT("The width of the scissor region.");
  int32_t Width = 0;
  DOCUMENT("The height of the scissor region.");
  int32_t Height = 0;
  DOCUMENT("``True`` if this scissor region is enabled.");
  bool Enabled = false;
};

DOCUMENT("Describes the rasterizer state toggles.");
struct RasterizerState
{
  DOCUMENT("The polygon fill mode.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon culling mode.");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool FrontCCW = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  float DepthBias = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float SlopeScaledDepthBias = 0.0f;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`DepthBias` and
:data:`SlopeScaledDepthBias`
)");
  float OffsetClamp = 0.0f;
  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clamped and
to ``0.0`` to ``1.0`` and not clipped.
)");
  bool DepthClamp = false;

  DOCUMENT("``True`` if multisampling should be used during rendering.");
  bool MultisampleEnable = false;
  DOCUMENT("``True`` if rendering should happen at sample-rate frequency.");
  bool SampleShading = false;
  DOCUMENT(R"(``True`` if the generated samples should be bitwise ``AND`` masked with
:data:`SampleMaskValue`.
)");
  bool SampleMask = false;
  DOCUMENT("The sample mask value that should be masked against the generated coverage.");
  uint32_t SampleMaskValue = ~0U;
  DOCUMENT(R"(``True`` if a temporary mask using :data:`SampleCoverageValue` should be used to
resolve the final output color.
)");
  bool SampleCoverage = false;
  DOCUMENT("``True`` if the temporary sample coverage mask should be inverted.");
  bool SampleCoverageInvert = false;
  DOCUMENT("The sample coverage value used if :data:`SampleCoverage` is ``True``.");
  float SampleCoverageValue = 1.0f;
  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool SampleAlphaToCoverage = false;
  DOCUMENT("``True`` if alpha-to-one should be used when blending to an MSAA target.");
  bool SampleAlphaToOne = false;
  DOCUMENT("The minimum sample shading rate.");
  float MinSampleShadingRate = 0.0f;

  DOCUMENT("``True`` if the point size can be programmably exported from a shader.");
  bool ProgrammablePointSize = false;
  DOCUMENT("The fixed point size in pixels.");
  float PointSize = 1.0f;
  DOCUMENT("The fixed line width in pixels.");
  float LineWidth = 1.0f;
  DOCUMENT("The threshold value at which points are clipped if they exceed this size.");
  float PointFadeThreshold = 0.0f;
  DOCUMENT("``True`` if the point sprite texture origin is upper-left. ``False`` if lower-left.");
  bool PointOriginUpperLeft = false;
};

DOCUMENT("Describes the rasterization state of the OpenGL pipeline.");
struct Rasterizer
{
  DOCUMENT("A list of :class:`GL_Viewport` with the bound viewports.");
  rdcarray<Viewport> Viewports;

  DOCUMENT("A list of :class:`GL_Scissor` with the bound scissor regions.");
  rdcarray<Scissor> Scissors;

  DOCUMENT("A :class:`GL_RasterizerState` with the details of the rasterization state.");
  RasterizerState m_State;
};

DOCUMENT("Describes the depth state.");
struct DepthState
{
  DOCUMENT("``True`` if depth testing should be performed.");
  bool DepthEnable = false;
  DOCUMENT("The :class:`CompareFunc` to use for testing depth values.");
  CompareFunc DepthFunc = CompareFunc::AlwaysTrue;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool DepthWrites = false;
  DOCUMENT("``True`` if depth bounds tests should be applied.");
  bool DepthBounds = false;
  DOCUMENT("The near plane bounding value.");
  double NearBound = 0.0;
  DOCUMENT("The far plane bounding value.");
  double FarBound = 0.0;
};

DOCUMENT("Describes the details of an OpenGL stencil operation.");
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
  DOCUMENT("The current stencil reference value.");
  uint8_t Ref = 0;
  DOCUMENT("The mask for testing stencil values.");
  uint8_t ValueMask = 0;
  DOCUMENT("The mask for writing stencil values.");
  uint8_t WriteMask = 0;
};

DOCUMENT("Describes the stencil state.");
struct StencilState
{
  DOCUMENT("``True`` if stencil operations should be performed.");
  bool StencilEnable = false;

  DOCUMENT("A :class:`GL_StencilFace` describing what happens for front-facing polygons.");
  StencilFace m_FrontFace;
  DOCUMENT("A :class:`GL_StencilFace` describing what happens for back-facing polygons.");
  StencilFace m_BackFace;
};

DOCUMENT("Describes the state of a framebuffer attachment.");
struct Attachment
{
  DOCUMENT("");
  bool operator==(const Attachment &o) const
  {
    return Obj == o.Obj && Layer == o.Layer && Mip == o.Mip && Swizzle[0] == o.Swizzle[0] &&
           Swizzle[1] == o.Swizzle[1] && Swizzle[2] == o.Swizzle[2] && Swizzle[3] == o.Swizzle[3];
  }
  bool operator<(const Attachment &o) const
  {
    if(!(Obj == o.Obj))
      return Obj < o.Obj;
    if(!(Layer == o.Layer))
      return Layer < o.Layer;
    if(!(Mip == o.Mip))
      return Mip < o.Mip;
    if(!(Swizzle[0] == o.Swizzle[0]))
      return Swizzle[0] < o.Swizzle[0];
    if(!(Swizzle[1] == o.Swizzle[1]))
      return Swizzle[1] < o.Swizzle[1];
    if(!(Swizzle[2] == o.Swizzle[2]))
      return Swizzle[2] < o.Swizzle[2];
    if(!(Swizzle[3] == o.Swizzle[3]))
      return Swizzle[3] < o.Swizzle[3];
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the texture bound to this attachment.");
  ResourceId Obj;
  DOCUMENT("The slice of the texture that's used in the attachment.");
  uint32_t Layer = 0;
  DOCUMENT("The mip of the texture that's used in the attachment.");
  uint32_t Mip = 0;
  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle Swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
};

DOCUMENT("Describes the contents of a framebuffer object.");
struct FBO
{
  DOCUMENT("The :class:`ResourceId` of the framebuffer.");
  ResourceId Obj;
  DOCUMENT("The list of :class:`GL_Attachment` with the framebuffer color attachments.");
  rdcarray<Attachment> Color;
  DOCUMENT("The :class:`GL_Attachment` with the framebuffer depth attachment.");
  Attachment Depth;
  DOCUMENT("The :class:`GL_Attachment` with the framebuffer stencil attachment.");
  Attachment Stencil;

  DOCUMENT("The list of draw buffer indices into the :data:`Color` attachment list.");
  rdcarray<int32_t> DrawBuffers;
  DOCUMENT("The read buffer index in the :data:`Color` attachment list.");
  int32_t ReadBuffer = 0;
};

DOCUMENT("Describes the details of an OpenGL blend operation.");
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

DOCUMENT("Describes the blend configuration for a given OpenGL attachment.");
struct Blend
{
  DOCUMENT("");
  bool operator==(const Blend &o) const
  {
    return Enabled == o.Enabled && m_Blend == o.m_Blend && m_AlphaBlend == o.m_AlphaBlend &&
           Logic == o.Logic && WriteMask == o.WriteMask;
  }
  bool operator<(const Blend &o) const
  {
    if(!(Enabled == o.Enabled))
      return Enabled < o.Enabled;
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
  DOCUMENT("A :class:`GL_BlendEquation` describing the blending for colour values.");
  BlendEquation m_Blend;
  DOCUMENT("A :class:`GL_BlendEquation` describing the blending for alpha values.");
  BlendEquation m_AlphaBlend;

  DOCUMENT("The :class:`LogicOp` to use for logic operations.");
  LogicOp Logic = LogicOp::NoOp;

  DOCUMENT("``True`` if blending is enabled for this target.");
  bool Enabled = false;
  DOCUMENT("The mask for writes to the render target.");
  byte WriteMask = 0;
};

DOCUMENT("Describes the blend pipeline state.");
struct BlendState
{
  DOCUMENT("A list of :class:`GL_Blend` describing the blend operations for each target.");
  rdcarray<Blend> Blends;

  DOCUMENT("The constant blend factor to use in blend equations.");
  float BlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the current state of the framebuffer stage of the pipeline.");
struct FrameBuffer
{
  DOCUMENT(
      "``True`` if sRGB correction should be applied when writing to an sRGB-formatted texture.");
  bool FramebufferSRGB = false;
  DOCUMENT("``True`` if dithering should be used when writing to color buffers.");
  bool Dither = false;

  DOCUMENT("A :class:`GL_FBO` with the information about a draw framebuffer.");
  FBO m_DrawFBO;
  DOCUMENT("A :class:`GL_FBO` with the information about a read framebuffer.");
  FBO m_ReadFBO;

  DOCUMENT("A :class:`GL_BlendState` with the details of the blending state.");
  BlendState m_Blending;
};

DOCUMENT("Describes the current state of GL hints and smoothing.");
struct Hints
{
  DOCUMENT("A :class:`QualityHint` with the derivatives hint.");
  QualityHint Derivatives = QualityHint::DontCare;
  DOCUMENT("A :class:`QualityHint` with the line smoothing hint.");
  QualityHint LineSmooth = QualityHint::DontCare;
  DOCUMENT("A :class:`QualityHint` with the polygon smoothing hint.");
  QualityHint PolySmooth = QualityHint::DontCare;
  DOCUMENT("A :class:`QualityHint` with the texture compression hint.");
  QualityHint TexCompression = QualityHint::DontCare;
  DOCUMENT("``True`` if line smoothing is enabled.");
  bool LineSmoothEnabled = false;
  DOCUMENT("``True`` if polygon smoothing is enabled.");
  bool PolySmoothEnabled = false;
};

DOCUMENT("The full current OpenGL pipeline state.");
struct State
{
  DOCUMENT("A :class:`GL_VertexInput` describing the vertex input stage.");
  VertexInput m_VtxIn;

  DOCUMENT("A :class:`GL_Shader` describing the vertex shader stage.");
  Shader m_VS;
  DOCUMENT("A :class:`GL_Shader` describing the tessellation control shader stage.");
  Shader m_TCS;
  DOCUMENT("A :class:`GL_Shader` describing the tessellation evaluation shader stage.");
  Shader m_TES;
  DOCUMENT("A :class:`GL_Shader` describing the geometry shader stage.");
  Shader m_GS;
  DOCUMENT("A :class:`GL_Shader` describing the fragment shader stage.");
  Shader m_FS;
  DOCUMENT("A :class:`GL_Shader` describing the compute shader stage.");
  Shader m_CS;

  DOCUMENT("The :class:`ResourceId` of the program pipeline (if active).");
  ResourceId Pipeline;

  DOCUMENT(
      "A :class:`GL_FixedVertexProcessing` describing the fixed-function vertex processing stage.");
  FixedVertexProcessing m_VtxProcess;

  DOCUMENT("A list of :class:`GL_Texture` with the currently bound textures.");
  rdcarray<Texture> Textures;
  DOCUMENT("A list of :class:`GL_Sampler` with the currently bound samplers.");
  rdcarray<Sampler> Samplers;

  DOCUMENT("A list of :class:`GL_Buffer` with the currently bound atomic buffers.");
  rdcarray<Buffer> AtomicBuffers;
  DOCUMENT("A list of :class:`GL_Buffer` with the currently bound uniform buffers.");
  rdcarray<Buffer> UniformBuffers;
  DOCUMENT("A list of :class:`GL_Buffer` with the currently bound shader storage buffers.");
  rdcarray<Buffer> ShaderStorageBuffers;

  DOCUMENT("A list of :class:`GL_ImageLoadStore` with the currently bound load/store images.");
  rdcarray<ImageLoadStore> Images;

  DOCUMENT("A :class:`GL_Feedback` describing the transform feedback stage.");
  Feedback m_Feedback;

  DOCUMENT("A :class:`GL_Rasterizer` describing rasterization.");
  Rasterizer m_Rasterizer;

  DOCUMENT("A :class:`GL_DepthState` describing depth processing.");
  DepthState m_DepthState;

  DOCUMENT("A :class:`GL_StencilState` describing stencil processing.");
  StencilState m_StencilState;

  DOCUMENT("A :class:`GL_FrameBuffer` describing the framebuffer.");
  FrameBuffer m_FB;

  DOCUMENT("A :class:`GL_Hints` describing the hint state.");
  Hints m_Hints;
};

};    // namespace GLPipe

DECLARE_REFLECTION_STRUCT(GLPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(GLPipe::VB);
DECLARE_REFLECTION_STRUCT(GLPipe::VertexInput);
DECLARE_REFLECTION_STRUCT(GLPipe::Shader);
DECLARE_REFLECTION_STRUCT(GLPipe::FixedVertexProcessing);
DECLARE_REFLECTION_STRUCT(GLPipe::Texture);
DECLARE_REFLECTION_STRUCT(GLPipe::Sampler);
DECLARE_REFLECTION_STRUCT(GLPipe::Buffer);
DECLARE_REFLECTION_STRUCT(GLPipe::ImageLoadStore);
DECLARE_REFLECTION_STRUCT(GLPipe::Feedback);
DECLARE_REFLECTION_STRUCT(GLPipe::Viewport);
DECLARE_REFLECTION_STRUCT(GLPipe::Scissor);
DECLARE_REFLECTION_STRUCT(GLPipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(GLPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(GLPipe::DepthState);
DECLARE_REFLECTION_STRUCT(GLPipe::StencilFace);
DECLARE_REFLECTION_STRUCT(GLPipe::StencilState);
DECLARE_REFLECTION_STRUCT(GLPipe::Attachment);
DECLARE_REFLECTION_STRUCT(GLPipe::FBO);
DECLARE_REFLECTION_STRUCT(GLPipe::BlendEquation);
DECLARE_REFLECTION_STRUCT(GLPipe::Blend);
DECLARE_REFLECTION_STRUCT(GLPipe::BlendState);
DECLARE_REFLECTION_STRUCT(GLPipe::FrameBuffer);
DECLARE_REFLECTION_STRUCT(GLPipe::Hints);
DECLARE_REFLECTION_STRUCT(GLPipe::State);
