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
  uint32_t Stride = 0;
  uint32_t Offset = 0;
};

struct IB
{
  ResourceId Buffer;
  uint32_t Offset = 0;
};

struct IA
{
  rdctype::array<Layout> layouts;
  ResourceId layout;
  ShaderReflection *Bytecode = NULL;
  bool32 customName = false;

  rdctype::str name;
  rdctype::array<VB> vbuffers;

  IB ibuffer;
};

struct View
{
  ResourceId Object;
  ResourceId Resource;
  rdctype::str Type;
  ResourceFormat Format;

  bool32 Structured = false;
  uint32_t BufferStructCount = 0;
  uint32_t ElementSize = 0;

  // Buffer (UAV)
  uint32_t FirstElement = 0;
  uint32_t NumElements = 1;

  // BufferEx
  D3DBufferViewFlags Flags = D3DBufferViewFlags::NoFlags;

  // Texture
  uint32_t HighestMip = 0;
  uint32_t NumMipLevels = 0;

  // Texture Array
  uint32_t ArraySize = 1;
  uint32_t FirstArraySlice = 0;
};

struct Sampler
{
  ResourceId Samp;
  rdctype::str name;
  bool32 customName = false;
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
  ResourceId Buffer;
  uint32_t VecOffset = 0;
  uint32_t VecCount = 0;
};

struct Shader
{
  ResourceId Object;
  rdctype::str name;
  bool32 customName = false;
  ShaderReflection *ShaderDetails = NULL;
  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage = ShaderStage::Vertex;

  rdctype::array<View> SRVs;
  rdctype::array<View> UAVs;

  rdctype::array<Sampler> Samplers;

  rdctype::array<CBuffer> ConstantBuffers;

  rdctype::array<rdctype::str> ClassInstances;
};

struct SOBind
{
  ResourceId Buffer;
  uint32_t Offset = 0;
};

struct SO
{
  rdctype::array<SOBind> Outputs;
};

struct Viewport
{
  Viewport() = default;
  Viewport(float TX, float TY, float W, float H, float MN, float MX, bool en)
      : X(TX), Y(TY), Width(W), Height(H), MinDepth(MN), MaxDepth(MX), Enabled(en)
  {
  }
  float X = 0.0f;
  float Y = 0.0f;
  float Width = 0.0f;
  float Height = 0.0f;
  float MinDepth = 0.0f;
  float MaxDepth = 0.0f;
  bool32 Enabled = false;
};

struct Scissor
{
  Scissor() = default;
  Scissor(int l, int t, int r, int b, bool en) : left(l), top(t), right(r), bottom(b), Enabled(en)
  {
  }

  int32_t left = 0;
  int32_t top = 0;
  int32_t right = 0;
  int32_t bottom = 0;
  bool32 Enabled = false;
};

struct RasterizerState
{
  ResourceId State;
  FillMode fillMode = FillMode::Solid;
  CullMode cullMode = CullMode::NoCull;
  bool32 FrontCCW = false;
  int32_t DepthBias = 0;
  float DepthBiasClamp = 0.0f;
  float SlopeScaledDepthBias = 0.0f;
  bool32 DepthClip = false;
  bool32 ScissorEnable = false;
  bool32 MultisampleEnable = false;
  bool32 AntialiasedLineEnable = false;
  uint32_t ForcedSampleCount = 0;
  bool32 ConservativeRasterization = false;
};

struct Rasterizer
{
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
  ResourceId State;
  bool32 DepthEnable = false;
  rdctype::str DepthFunc;
  bool32 DepthWrites = false;
  bool32 StencilEnable = false;
  byte StencilReadMask = 0;
  byte StencilWriteMask = 0;

  StencilOp m_FrontFace, m_BackFace;

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
  BlendOp m_Blend, m_AlphaBlend;

  rdctype::str LogicOp;

  bool32 Enabled = false;
  bool32 LogicEnabled = false;
  byte WriteMask = 0;
};

struct BlendState
{
  ResourceId State;

  bool32 AlphaToCoverage = false;
  bool32 IndependentBlend = false;

  rdctype::array<Blend> Blends;

  float BlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  uint32_t SampleMask = ~0U;
};

struct OM
{
  DepthStencilState m_State;
  BlendState m_BlendState;

  rdctype::array<View> RenderTargets;

  uint32_t UAVStartSlot = 0;
  rdctype::array<View> UAVs;

  View DepthTarget;
  bool32 DepthReadOnly = false;
  bool32 StencilReadOnly = false;
};

struct State
{
  IA m_IA;

  Shader m_VS, m_HS, m_DS, m_GS, m_PS, m_CS;

  SO m_SO;

  Rasterizer m_RS;

  OM m_OM;
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
DECLARE_REFLECTION_STRUCT(D3D11Pipe::StencilOp);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::Blend);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::BlendOp);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::OM);
DECLARE_REFLECTION_STRUCT(D3D11Pipe::State);
