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
struct Layout
{
  rdctype::str SemanticName;
  uint32_t SemanticIndex = 0;
  ResourceFormat Format;
  uint32_t InputSlot = 0;
  uint32_t ByteOffset = 0;
  bool32 PerInstance = false;
  uint32_t InstanceDataStepRate = 0;
};

struct VB
{
  ResourceId Buffer;
  uint64_t Offset = 0;
  uint32_t Size = 0;
  uint32_t Stride = 0;
};

struct IB
{
  ResourceId Buffer;
  uint64_t Offset = 0;
  uint32_t Size = 0;
};

struct IA
{
  rdctype::array<Layout> layouts;

  rdctype::array<VB> vbuffers;

  IB ibuffer;

  uint32_t indexStripCutValue = 0;
};

// Immediate indicates either a root parameter (not in a table), or static samplers
// RootElement is the index in the original root signature that this descriptor came from.

struct View
{
  bool32 Immediate = false;
  uint32_t RootElement = ~0U;
  uint32_t TableIndex = ~0U;

  ResourceId Resource;
  rdctype::str Type;
  ResourceFormat Format;

  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  D3DBufferViewFlags BufferFlags = D3DBufferViewFlags::NoFlags;
  uint32_t BufferStructCount = 0;
  uint32_t ElementSize = 0;
  uint64_t FirstElement = 0;
  uint32_t NumElements = 1;

  ResourceId CounterResource;
  uint64_t CounterByteOffset = 0;

  uint32_t HighestMip = 0;
  uint32_t NumMipLevels = 1;

  uint32_t ArraySize = 1;
  uint32_t FirstArraySlice = 0;

  float MinLODClamp = 0.0f;
};

struct Sampler
{
  bool32 Immediate = 0;
  uint32_t RootElement = ~0U;
  uint32_t TableIndex = ~0U;

  rdctype::str AddressU;
  rdctype::str AddressV;
  rdctype::str AddressW;
  float BorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  rdctype::str Comparison;
  rdctype::str Filter;
  bool32 UseBorder = false;
  bool32 UseComparison = false;
  uint32_t MaxAniso = 0;
  float MaxLOD = 0.0f;
  float MinLOD = 0.0f;
  float MipLODBias = 0.0f;
};

struct CBuffer
{
  bool32 Immediate = false;
  uint32_t RootElement = ~0U;
  uint32_t TableIndex = ~0U;

  ResourceId Buffer;
  uint64_t Offset = 0;
  uint32_t ByteSize = 0;

  rdctype::array<uint32_t> RootValues;
};

struct RegisterSpace
{
  rdctype::array<CBuffer> ConstantBuffers;
  rdctype::array<Sampler> Samplers;
  rdctype::array<View> SRVs;
  rdctype::array<View> UAVs;
};

struct Shader
{
  ResourceId Object;

  ShaderReflection *ShaderDetails = NULL;

  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage = ShaderStage::Vertex;

  rdctype::array<RegisterSpace> Spaces;
};

struct SOBind
{
  ResourceId Buffer;
  uint64_t Offset = 0;
  uint64_t Size = 0;

  ResourceId WrittenCountBuffer;
  uint64_t WrittenCountOffset = 0;
};

struct Streamout
{
  rdctype::array<SOBind> Outputs;
};

struct Viewport
{
  Viewport() = default;
  Viewport(float TX, float TY, float W, float H, float MN, float MX)
      : X(TX), Y(TY), Width(W), Height(H), MinDepth(MN), MaxDepth(MX)
  {
  }

  float X = 0.0f;
  float Y = 0.0f;
  float Width = 0.0f;
  float Height = 0.0f;
  float MinDepth = 0.0f;
  float MaxDepth = 0.0f;
};

struct Scissor
{
  Scissor() = default;
  Scissor(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
  int32_t left = 0;
  int32_t top = 0;
  int32_t right = 0;
  int32_t bottom = 0;
};

struct RasterizerState
{
  FillMode fillMode = FillMode::Solid;
  CullMode cullMode = CullMode::NoCull;
  bool32 FrontCCW = false;
  int32_t DepthBias = 0;
  float DepthBiasClamp = 0.0f;
  float SlopeScaledDepthBias = 0.0f;
  bool32 DepthClip = false;
  bool32 MultisampleEnable = false;
  bool32 AntialiasedLineEnable = false;
  uint32_t ForcedSampleCount = 0;
  bool32 ConservativeRasterization = false;
};

struct Rasterizer
{
  uint32_t SampleMask = ~0U;

  rdctype::array<Viewport> Viewports;

  rdctype::array<Scissor> Scissors;

  RasterizerState m_State;
};

struct StencilOp
{
  rdctype::str FailOp;
  rdctype::str DepthFailOp;
  rdctype::str PassOp;
  rdctype::str Func;
};

struct DepthStencilState
{
  bool32 DepthEnable = false;
  bool32 DepthWrites = false;
  rdctype::str DepthFunc;
  bool32 StencilEnable = false;
  byte StencilReadMask = 0;
  byte StencilWriteMask = 0;

  StencilOp m_FrontFace;
  StencilOp m_BackFace;

  uint32_t StencilRef = 0;
};

struct BlendOp
{
  rdctype::str Source;
  rdctype::str Destination;
  rdctype::str Operation;
};

struct Blend
{
  BlendOp m_Blend;
  BlendOp m_AlphaBlend;

  rdctype::str LogicOp;

  bool32 Enabled = false;
  bool32 LogicEnabled = false;
  byte WriteMask = 0;
};

struct BlendState
{
  bool32 AlphaToCoverage = false;
  bool32 IndependentBlend = false;
  rdctype::array<Blend> Blends;

  float BlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct OM
{
  DepthStencilState m_State;
  BlendState m_BlendState;

  rdctype::array<View> RenderTargets;

  View DepthTarget;
  bool32 DepthReadOnly = false;
  bool32 StencilReadOnly = false;

  uint32_t multiSampleCount = 1;
  uint32_t multiSampleQuality = 0;
};

struct ResourceState
{
  rdctype::str name;
};

struct ResourceData
{
  ResourceId id;

  rdctype::array<ResourceState> states;
};

struct State
{
  ResourceId pipeline;
  bool32 customName = false;
  rdctype::str name;

  ResourceId rootSig;

  IA m_IA;

  Shader m_VS;
  Shader m_HS;
  Shader m_DS;
  Shader m_GS;
  Shader m_PS;
  Shader m_CS;

  Streamout m_SO;

  Rasterizer m_RS;

  OM m_OM;

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
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StencilOp);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::DepthStencilState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::BlendOp);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Blend);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::OM);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceData);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::State);
