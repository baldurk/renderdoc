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
struct VertexAttribute
{
  VertexAttribute() : BufferSlot(0), RelativeOffset(0) {}
  bool32 Enabled;
  ResourceFormat Format;

  PixelValue GenericValue;

  uint32_t BufferSlot;
  uint32_t RelativeOffset;
};

struct VB
{
  VB() : Buffer(), Stride(0), Offset(0), Divisor(0) {}
  ResourceId Buffer;
  uint32_t Stride;
  uint32_t Offset;
  uint32_t Divisor;
};

struct VertexInput
{
  VertexInput() : ibuffer(), primitiveRestart(false), restartIndex(0), provokingVertexLast(false) {}
  rdctype::array<VertexAttribute> attributes;

  rdctype::array<VB> vbuffers;

  ResourceId ibuffer;
  bool32 primitiveRestart;
  uint32_t restartIndex;

  bool32 provokingVertexLast;
};

struct Shader
{
  Shader()
      : Object(),
        customShaderName(false),
        customProgramName(false),
        PipelineActive(false),
        customPipelineName(false),
        ShaderDetails(NULL),
        stage(ShaderStage::Vertex)
  {
  }
  ResourceId Object;

  rdctype::str ShaderName;
  bool32 customShaderName;

  rdctype::str ProgramName;
  bool32 customProgramName;

  bool32 PipelineActive;
  rdctype::str PipelineName;
  bool32 customPipelineName;

  ShaderReflection *ShaderDetails;
  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage;

  rdctype::array<uint32_t> Subroutines;
};

struct FixedVertexProcessing
{
  FixedVertexProcessing() : discard(0), clipOriginLowerLeft(0), clipNegativeOneToOne(0)
  {
    defaultInnerLevel[0] = defaultInnerLevel[1] = 0.0f;
    defaultOuterLevel[0] = defaultOuterLevel[1] = defaultOuterLevel[2] = defaultOuterLevel[3] = 0.0f;
    clipPlanes[0] = clipPlanes[1] = clipPlanes[2] = clipPlanes[3] = clipPlanes[4] = clipPlanes[5] =
        clipPlanes[6] = clipPlanes[7] = 0;
  }

  float defaultInnerLevel[2];
  float defaultOuterLevel[4];
  bool32 discard;

  bool32 clipPlanes[8];
  bool32 clipOriginLowerLeft;
  bool32 clipNegativeOneToOne;
};

struct Texture
{
  Texture() : Resource(), FirstSlice(0), ResType(TextureDim::Unknown), DepthReadChannel(-1)
  {
    Swizzle[0] = TextureSwizzle::Red;
    Swizzle[1] = TextureSwizzle::Green;
    Swizzle[2] = TextureSwizzle::Blue;
    Swizzle[3] = TextureSwizzle::Alpha;
  }
  ResourceId Resource;
  uint32_t FirstSlice;
  uint32_t HighestMip;
  TextureDim ResType;

  TextureSwizzle Swizzle[4];
  int32_t DepthReadChannel;
};

struct Sampler
{
  Sampler()
      : Samp(),
        UseBorder(false),
        UseComparison(false),
        SeamlessCube(false),
        MaxAniso(0.0f),
        MaxLOD(0.0f),
        MinLOD(0.0f),
        MipLODBias(0.0f)
  {
    BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 0.0f;
  }
  ResourceId Samp;
  rdctype::str AddressS, AddressT, AddressR;
  float BorderColor[4];
  rdctype::str Comparison;
  rdctype::str MinFilter;
  rdctype::str MagFilter;
  bool32 UseBorder;
  bool32 UseComparison;
  bool32 SeamlessCube;
  float MaxAniso;
  float MaxLOD;
  float MinLOD;
  float MipLODBias;
};

struct Buffer
{
  Buffer() : Resource(), Offset(0), Size(0) {}
  ResourceId Resource;
  uint64_t Offset;
  uint64_t Size;
};

struct ImageLoadStore
{
  ImageLoadStore()
      : Resource(),
        Level(0),
        Layered(false),
        Layer(0),
        ResType(TextureDim::Unknown),
        readAllowed(false),
        writeAllowed(false)
  {
  }
  ResourceId Resource;
  uint32_t Level;
  bool32 Layered;
  uint32_t Layer;
  TextureDim ResType;
  bool32 readAllowed;
  bool32 writeAllowed;
  ResourceFormat Format;
};

struct Feedback
{
  Feedback() : Active(false), Paused(false)
  {
    Offset[0] = Offset[1] = Offset[2] = Offset[3] = 0;
    Size[0] = Size[1] = Size[2] = Size[3] = 0;
  }

  ResourceId Obj;
  ResourceId BufferBinding[4];
  uint64_t Offset[4];
  uint64_t Size[4];
  bool32 Active;
  bool32 Paused;
};

struct Viewport
{
  Viewport() : Left(0.0f), Bottom(0.0f), Width(0.0f), Height(0.0f), MinDepth(0.0f), MaxDepth(0.0f)
  {
  }
  float Left, Bottom;
  float Width, Height;
  double MinDepth, MaxDepth;
};

struct Scissor
{
  Scissor() : Left(0), Bottom(0), Width(0), Height(0), Enabled(false) {}
  int32_t Left, Bottom;
  int32_t Width, Height;
  bool32 Enabled;
};

struct RasterizerState
{
  RasterizerState()
      : fillMode(FillMode::Solid),
        cullMode(CullMode::NoCull),
        FrontCCW(false),
        DepthBias(0),
        SlopeScaledDepthBias(0.0f),
        OffsetClamp(0.0f),
        DepthClamp(false),
        MultisampleEnable(false),
        SampleShading(false),
        SampleMask(false),
        SampleMaskValue(~0U),
        SampleCoverage(false),
        SampleCoverageInvert(false),
        SampleCoverageValue(1.0f),
        SampleAlphaToCoverage(false),
        SampleAlphaToOne(false),
        MinSampleShadingRate(0.0f),
        ProgrammablePointSize(false),
        PointSize(1.0f),
        LineWidth(1.0f),
        PointFadeThreshold(0.0f),
        PointOriginUpperLeft(false)
  {
  }
  FillMode fillMode;
  CullMode cullMode;
  bool32 FrontCCW;
  float DepthBias;
  float SlopeScaledDepthBias;
  float OffsetClamp;
  bool32 DepthClamp;

  bool32 MultisampleEnable;
  bool32 SampleShading;
  bool32 SampleMask;
  uint32_t SampleMaskValue;
  bool32 SampleCoverage;
  bool32 SampleCoverageInvert;
  float SampleCoverageValue;
  bool32 SampleAlphaToCoverage;
  bool32 SampleAlphaToOne;
  float MinSampleShadingRate;

  bool32 ProgrammablePointSize;
  float PointSize;
  float LineWidth;
  float PointFadeThreshold;
  bool32 PointOriginUpperLeft;
};

struct Rasterizer
{
  rdctype::array<Viewport> Viewports;

  rdctype::array<Scissor> Scissors;

  RasterizerState m_State;
};

struct DepthState
{
  DepthState()
      : DepthEnable(false), DepthWrites(false), DepthBounds(false), NearBound(0), FarBound(0)
  {
  }
  bool32 DepthEnable;
  rdctype::str DepthFunc;
  bool32 DepthWrites;
  bool32 DepthBounds;
  double NearBound, FarBound;
};

struct StencilOp
{
  StencilOp() : Ref(0), ValueMask(0), WriteMask(0) {}
  rdctype::str FailOp;
  rdctype::str DepthFailOp;
  rdctype::str PassOp;
  rdctype::str Func;
  uint32_t Ref;
  uint32_t ValueMask;
  uint32_t WriteMask;
};

struct StencilState
{
  StencilState() : StencilEnable(false) {}
  bool32 StencilEnable;

  StencilOp m_FrontFace, m_BackFace;
};

struct Attachment
{
  Attachment() : Obj(), Layer(0), Mip(0)
  {
    Swizzle[0] = TextureSwizzle::Red;
    Swizzle[1] = TextureSwizzle::Green;
    Swizzle[2] = TextureSwizzle::Blue;
    Swizzle[3] = TextureSwizzle::Alpha;
  }
  ResourceId Obj;
  uint32_t Layer;
  uint32_t Mip;
  TextureSwizzle Swizzle[4];
};

struct FBO
{
  FBO() : Obj(), Depth(), Stencil(), ReadBuffer(0) {}
  ResourceId Obj;
  rdctype::array<Attachment> Color;
  Attachment Depth;
  Attachment Stencil;

  rdctype::array<int32_t> DrawBuffers;
  int32_t ReadBuffer;
};

struct BlendOp
{
  rdctype::str Source;
  rdctype::str Destination;
  rdctype::str Operation;
};

struct Blend
{
  Blend() : Enabled(false), WriteMask(0) {}
  BlendOp m_Blend, m_AlphaBlend;

  rdctype::str LogicOp;

  bool32 Enabled;
  byte WriteMask;
};

struct BlendState
{
  BlendState() { BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 0.0f; }
  rdctype::array<Blend> Blends;

  float BlendFactor[4];
};

struct FrameBuffer
{
  FrameBuffer() : FramebufferSRGB(false), Dither(false) {}
  bool32 FramebufferSRGB;
  bool32 Dither;

  FBO m_DrawFBO, m_ReadFBO;

  BlendState m_Blending;
};

struct Hints
{
  Hints()
      : Derivatives(QualityHint::DontCare),
        LineSmooth(QualityHint::DontCare),
        PolySmooth(QualityHint::DontCare),
        TexCompression(QualityHint::DontCare),
        LineSmoothEnabled(0),
        PolySmoothEnabled(0)
  {
  }

  QualityHint Derivatives;
  QualityHint LineSmooth;
  QualityHint PolySmooth;
  QualityHint TexCompression;
  bool32 LineSmoothEnabled;
  bool32 PolySmoothEnabled;
};

struct State
{
  State() {}
  VertexInput m_VtxIn;

  Shader m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;

  FixedVertexProcessing m_VtxProcess;

  rdctype::array<Texture> Textures;
  rdctype::array<Sampler> Samplers;

  rdctype::array<Buffer> AtomicBuffers;
  rdctype::array<Buffer> UniformBuffers;
  rdctype::array<Buffer> ShaderStorageBuffers;

  rdctype::array<ImageLoadStore> Images;

  Feedback m_Feedback;

  Rasterizer m_Rasterizer;

  DepthState m_DepthState;

  StencilState m_StencilState;

  FrameBuffer m_FB;

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
DECLARE_REFLECTION_STRUCT(GLPipe::StencilOp);
DECLARE_REFLECTION_STRUCT(GLPipe::StencilState);
DECLARE_REFLECTION_STRUCT(GLPipe::Attachment);
DECLARE_REFLECTION_STRUCT(GLPipe::FBO);
DECLARE_REFLECTION_STRUCT(GLPipe::BlendOp);
DECLARE_REFLECTION_STRUCT(GLPipe::Blend);
DECLARE_REFLECTION_STRUCT(GLPipe::BlendState);
DECLARE_REFLECTION_STRUCT(GLPipe::FrameBuffer);
DECLARE_REFLECTION_STRUCT(GLPipe::Hints);
DECLARE_REFLECTION_STRUCT(GLPipe::State);
