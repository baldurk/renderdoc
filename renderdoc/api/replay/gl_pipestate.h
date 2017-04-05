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
  bool32 Enabled = false;
  ResourceFormat Format;

  PixelValue GenericValue;

  uint32_t BufferSlot = 0;
  uint32_t RelativeOffset = 0;
};

struct VB
{
  ResourceId Buffer;
  uint32_t Stride = 0;
  uint32_t Offset = 0;
  uint32_t Divisor = 0;
};

struct VertexInput
{
  rdctype::array<VertexAttribute> attributes;

  rdctype::array<VB> vbuffers;

  ResourceId ibuffer;
  bool32 primitiveRestart = false;
  uint32_t restartIndex = 0;

  bool32 provokingVertexLast = false;
};

struct Shader
{
  ResourceId Object;

  rdctype::str ShaderName;
  bool32 customShaderName = false;

  rdctype::str ProgramName;
  bool32 customProgramName = false;

  bool32 PipelineActive = false;
  rdctype::str PipelineName;
  bool32 customPipelineName = false;

  ShaderReflection *ShaderDetails = NULL;
  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage = ShaderStage::Vertex;

  rdctype::array<uint32_t> Subroutines;
};

struct FixedVertexProcessing
{
  float defaultInnerLevel[2] = {0.0f, 0.0f};
  float defaultOuterLevel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool32 discard = false;

  bool32 clipPlanes[8] = {false, false, false, false, false, false, false, false};
  bool32 clipOriginLowerLeft = false;
  bool32 clipNegativeOneToOne = false;
};

struct Texture
{
  ResourceId Resource;
  uint32_t FirstSlice = 0;
  uint32_t HighestMip = 0;
  TextureDim ResType = TextureDim::Unknown;

  TextureSwizzle Swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  int32_t DepthReadChannel = -1;
};

struct Sampler
{
  ResourceId Samp;
  rdctype::str AddressS, AddressT, AddressR;
  float BorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  rdctype::str Comparison;
  rdctype::str MinFilter;
  rdctype::str MagFilter;
  bool32 UseBorder = false;
  bool32 UseComparison = false;
  bool32 SeamlessCube = false;
  float MaxAniso = 0.0f;
  float MaxLOD = 0.0f;
  float MinLOD = 0.0f;
  float MipLODBias = 0.0f;
};

struct Buffer
{
  ResourceId Resource;
  uint64_t Offset = 0;
  uint64_t Size = 0;
};

struct ImageLoadStore
{
  ResourceId Resource;
  uint32_t Level = 0;
  bool32 Layered = false;
  uint32_t Layer = 0;
  TextureDim ResType = TextureDim::Unknown;
  bool32 readAllowed = false;
  bool32 writeAllowed = false;
  ResourceFormat Format;
};

struct Feedback
{
  ResourceId Obj;
  ResourceId BufferBinding[4];
  uint64_t Offset[4] = {0, 0, 0, 0};
  uint64_t Size[4] = {0, 0, 0, 0};
  bool32 Active = false;
  bool32 Paused = false;
};

struct Viewport
{
  float Left = 0.0f;
  float Bottom = 0.0f;
  float Width = 0.0f;
  float Height = 0.0f;
  double MinDepth = 0.0;
  double MaxDepth = 0.0;
};

struct Scissor
{
  int32_t Left = 0;
  int32_t Bottom = 0;
  int32_t Width = 0;
  int32_t Height = 0;
  bool32 Enabled = false;
};

struct RasterizerState
{
  FillMode fillMode = FillMode::Solid;
  CullMode cullMode = CullMode::NoCull;
  bool32 FrontCCW = false;
  float DepthBias = 0.0f;
  float SlopeScaledDepthBias = 0.0f;
  float OffsetClamp = 0.0f;
  bool32 DepthClamp = false;

  bool32 MultisampleEnable = false;
  bool32 SampleShading = false;
  bool32 SampleMask = false;
  uint32_t SampleMaskValue = ~0U;
  bool32 SampleCoverage = false;
  bool32 SampleCoverageInvert = false;
  float SampleCoverageValue = 1.0f;
  bool32 SampleAlphaToCoverage = false;
  bool32 SampleAlphaToOne = false;
  float MinSampleShadingRate = 0.0f;

  bool32 ProgrammablePointSize = false;
  float PointSize = 1.0f;
  float LineWidth = 1.0f;
  float PointFadeThreshold = 0.0f;
  bool32 PointOriginUpperLeft = false;
};

struct Rasterizer
{
  rdctype::array<Viewport> Viewports;

  rdctype::array<Scissor> Scissors;

  RasterizerState m_State;
};

struct DepthState
{
  bool32 DepthEnable = false;
  rdctype::str DepthFunc;
  bool32 DepthWrites = false;
  bool32 DepthBounds = false;
  double NearBound = 0.0;
  double FarBound = 0.0;
};

struct StencilOp
{
  rdctype::str FailOp;
  rdctype::str DepthFailOp;
  rdctype::str PassOp;
  rdctype::str Func;
  uint32_t Ref = 0;
  uint32_t ValueMask = 0;
  uint32_t WriteMask = 0;
};

struct StencilState
{
  bool32 StencilEnable = false;

  StencilOp m_FrontFace, m_BackFace;
};

struct Attachment
{
  ResourceId Obj;
  uint32_t Layer = 0;
  uint32_t Mip = 0;
  TextureSwizzle Swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
};

struct FBO
{
  ResourceId Obj;
  rdctype::array<Attachment> Color;
  Attachment Depth;
  Attachment Stencil;

  rdctype::array<int32_t> DrawBuffers;
  int32_t ReadBuffer = 0;
};

struct BlendOp
{
  rdctype::str Source;
  rdctype::str Destination;
  rdctype::str Operation;
};

struct Blend
{
  BlendOp m_Blend, m_AlphaBlend;

  rdctype::str LogicOp;

  bool32 Enabled = false;
  byte WriteMask = 0;
};

struct BlendState
{
  rdctype::array<Blend> Blends;

  float BlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct FrameBuffer
{
  bool32 FramebufferSRGB = false;
  bool32 Dither = false;

  FBO m_DrawFBO, m_ReadFBO;

  BlendState m_Blending;
};

struct Hints
{
  QualityHint Derivatives = QualityHint::DontCare;
  QualityHint LineSmooth = QualityHint::DontCare;
  QualityHint PolySmooth = QualityHint::DontCare;
  QualityHint TexCompression = QualityHint::DontCare;
  bool32 LineSmoothEnabled = false;
  bool32 PolySmoothEnabled = false;
};

struct State
{
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
