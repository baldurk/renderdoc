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
  Layout()
      : SemanticIndex(0), InputSlot(0), ByteOffset(0), PerInstance(false), InstanceDataStepRate(0)
  {
  }
  rdctype::str SemanticName;
  uint32_t SemanticIndex;
  ResourceFormat Format;
  uint32_t InputSlot;
  uint32_t ByteOffset;
  bool32 PerInstance;
  uint32_t InstanceDataStepRate;
};

struct VB
{
  VB() : Buffer(), Offset(0), Stride(0) {}
  ResourceId Buffer;
  uint64_t Offset;
  uint32_t Size;
  uint32_t Stride;
};

struct IB
{
  IB() : Buffer(), Offset(0) {}
  ResourceId Buffer;
  uint64_t Offset;
  uint32_t Size;
};

struct IA
{
  IA() : indexStripCutValue(0) {}
  rdctype::array<Layout> layouts;

  rdctype::array<VB> vbuffers;

  IB ibuffer;

  uint32_t indexStripCutValue;
};

// Immediate indicates either a root parameter (not in a table), or static samplers
// RootElement is the index in the original root signature that this descriptor came from.

struct View
{
  View()
      : Immediate(0),
        RootElement(~0U),
        TableIndex(~0U),
        Resource(),
        Format(),
        BufferFlags(D3DBufferViewFlags::NoFlags),
        BufferStructCount(0),
        ElementSize(0),
        FirstElement(0),
        NumElements(1),
        CounterByteOffset(0),
        HighestMip(0),
        NumMipLevels(1),
        ArraySize(1),
        FirstArraySlice(0),
        MinLODClamp(0.0f)
  {
    swizzle[0] = TextureSwizzle::Red;
    swizzle[1] = TextureSwizzle::Green;
    swizzle[2] = TextureSwizzle::Blue;
    swizzle[3] = TextureSwizzle::Alpha;
  }

  // parameters from descriptor
  bool32 Immediate;
  uint32_t RootElement;
  uint32_t TableIndex;

  // parameters from resource/view
  ResourceId Resource;
  rdctype::str Type;
  ResourceFormat Format;

  TextureSwizzle swizzle[4];
  D3DBufferViewFlags BufferFlags;
  uint32_t BufferStructCount;
  uint32_t ElementSize;
  uint64_t FirstElement;
  uint32_t NumElements;

  ResourceId CounterResource;
  uint64_t CounterByteOffset;

  // Texture
  uint32_t HighestMip;
  uint32_t NumMipLevels;

  // Texture Array
  uint32_t ArraySize;
  uint32_t FirstArraySlice;

  float MinLODClamp;
};

struct Sampler
{
  Sampler()
      : Immediate(0),
        RootElement(~0U),
        TableIndex(~0U),
        UseBorder(false),
        UseComparison(false),
        MaxAniso(0),
        MaxLOD(0.0f),
        MinLOD(0.0f),
        MipLODBias(0.0f)
  {
    BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 0.0f;
  }

  // parameters from descriptor
  bool32 Immediate;
  uint32_t RootElement;
  uint32_t TableIndex;

  // parameters from resource/view
  rdctype::str AddressU, AddressV, AddressW;
  float BorderColor[4];
  rdctype::str Comparison;
  rdctype::str Filter;
  bool32 UseBorder;
  bool32 UseComparison;
  uint32_t MaxAniso;
  float MaxLOD;
  float MinLOD;
  float MipLODBias;
};

struct CBuffer
{
  CBuffer() : Immediate(0), RootElement(~0U), TableIndex(~0U), Buffer(), Offset(0), ByteSize(0) {}
  // parameters from descriptor
  bool32 Immediate;
  uint32_t RootElement;
  uint32_t TableIndex;

  // parameters from resource/view
  ResourceId Buffer;
  uint64_t Offset;
  uint32_t ByteSize;

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
  Shader() : ShaderDetails(NULL), stage(ShaderStage::Vertex) {}
  ResourceId Object;
  ShaderReflection *ShaderDetails;
  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage;

  rdctype::array<RegisterSpace> Spaces;
};

struct SOBind
{
  SOBind() : Buffer(), Offset(0), Size(0), WrittenCountBuffer(), WrittenCountOffset(0) {}
  ResourceId Buffer;
  uint64_t Offset;
  uint64_t Size;

  ResourceId WrittenCountBuffer;
  uint64_t WrittenCountOffset;
};

struct Streamout
{
  rdctype::array<SOBind> Outputs;
};

struct Viewport
{
  Viewport() : X(0.0f), Y(0.0f), Width(0.0f), Height(0.0f), MinDepth(0.0f), MaxDepth(0.0f) {}
  Viewport(float TX, float TY, float W, float H, float MN, float MX)
      : X(TX), Y(TY), Width(W), Height(H), MinDepth(MN), MaxDepth(MX)
  {
  }
  float X;
  float Y;
  float Width;
  float Height;
  float MinDepth;
  float MaxDepth;
};

struct Scissor
{
  Scissor() : left(0), top(0), right(0), bottom(0) {}
  Scissor(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
  int32_t left, top, right, bottom;
};

struct RasterizerState
{
  RasterizerState()
          : fillMode(FillMode::Solid),
            cullMode(CullMode::NoCull),
        FrontCCW(false),
        DepthBias(0),
        DepthBiasClamp(0.0f),
        SlopeScaledDepthBias(0.0f),
        DepthClip(false),
        MultisampleEnable(false),
        AntialiasedLineEnable(false),
        ForcedSampleCount(0),
        ConservativeRasterization(false)
  {
  }
      FillMode fillMode;
      CullMode cullMode;
  bool32 FrontCCW;
  int32_t DepthBias;
  float DepthBiasClamp;
  float SlopeScaledDepthBias;
  bool32 DepthClip;
  bool32 MultisampleEnable;
  bool32 AntialiasedLineEnable;
  uint32_t ForcedSampleCount;
  bool32 ConservativeRasterization;
};

struct Rasterizer
{
  Rasterizer() : SampleMask(~0U) {}
  uint32_t SampleMask;

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
  DepthStencilState()
      : DepthEnable(false),
        DepthWrites(false),
        StencilEnable(false),
        StencilReadMask(0),
        StencilWriteMask(0),
        StencilRef(0)
  {
  }
  bool32 DepthEnable;
  bool32 DepthWrites;
  rdctype::str DepthFunc;
  bool32 StencilEnable;
  byte StencilReadMask;
  byte StencilWriteMask;

  StencilOp m_FrontFace, m_BackFace;

  uint32_t StencilRef;
};

struct BlendOp
{
  rdctype::str Source;
  rdctype::str Destination;
  rdctype::str Operation;
};

struct Blend
{
  Blend() : Enabled(false), LogicEnabled(false), WriteMask(0) {}
  BlendOp m_Blend, m_AlphaBlend;

  rdctype::str LogicOp;

  bool32 Enabled;
  bool32 LogicEnabled;
  byte WriteMask;
};

struct BlendState
{
  BlendState() : AlphaToCoverage(false), IndependentBlend(false)
  {
    BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 0.0f;
  }

  bool32 AlphaToCoverage;
  bool32 IndependentBlend;
  rdctype::array<Blend> Blends;

  float BlendFactor[4];
};

struct OM
{
  OM() : DepthReadOnly(false), StencilReadOnly(false), multiSampleCount(1), multiSampleQuality(0) {}
  DepthStencilState m_State;
  BlendState m_BlendState;

  rdctype::array<View> RenderTargets;

  View DepthTarget;
  bool32 DepthReadOnly;
  bool32 StencilReadOnly;

  uint32_t multiSampleCount;
  uint32_t multiSampleQuality;
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
  State() : customName(false) {}
  ResourceId pipeline;
  bool32 customName;
  rdctype::str name;

  ResourceId rootSig;

  IA m_IA;

  Shader m_VS, m_HS, m_DS, m_GS, m_PS, m_CS;

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
