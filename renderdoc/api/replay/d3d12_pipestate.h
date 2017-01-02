/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

struct D3D12PipelineState
{
  D3D12PipelineState() : customName(false) {}
  ResourceId pipeline;
  bool32 customName;
  rdctype::str PipelineName;

  ResourceId rootSig;

  struct InputAssembler
  {
    InputAssembler() : indexStripCutValue(0) {}
    struct LayoutInput
    {
      LayoutInput()
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
    rdctype::array<LayoutInput> layouts;

    struct VertexBuffer
    {
      VertexBuffer() : Buffer(), Offset(0), Stride(0) {}
      ResourceId Buffer;
      uint64_t Offset;
      uint32_t Size;
      uint32_t Stride;
    };
    rdctype::array<VertexBuffer> vbuffers;

    struct IndexBuffer
    {
      IndexBuffer() : Buffer(), Offset(0) {}
      ResourceId Buffer;
      uint64_t Offset;
      uint32_t Size;
    } ibuffer;

    uint32_t indexStripCutValue;
  } m_IA;

  // Immediate indicates either a root parameter (not in a table), or static samplers
  // RootElement is the index in the original root signature that this descriptor came from.

  struct ResourceView
  {
    ResourceView()
        : Immediate(0),
          RootElement(~0U),
          TableIndex(~0U),
          Resource(),
          Format(),
          BufferFlags(0),
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
      swizzle[0] = eSwizzle_Red;
      swizzle[1] = eSwizzle_Green;
      swizzle[2] = eSwizzle_Blue;
      swizzle[3] = eSwizzle_Alpha;
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
    uint32_t BufferFlags;
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

  struct ShaderStage
  {
    ShaderStage() : ShaderDetails(NULL), stage(eShaderStage_Vertex) {}
    ResourceId Shader;
    ShaderReflection *ShaderDetails;
    ShaderBindpointMapping BindpointMapping;

    ShaderStageType stage;

    struct RegisterSpace
    {
      rdctype::array<CBuffer> ConstantBuffers;
      rdctype::array<Sampler> Samplers;
      rdctype::array<ResourceView> SRVs;
      rdctype::array<ResourceView> UAVs;
    };
    rdctype::array<RegisterSpace> Spaces;
  } m_VS, m_HS, m_DS, m_GS, m_PS, m_CS;

  struct Streamout
  {
    struct Output
    {
      Output() : Buffer(), Offset(0), Size(0), WrittenCountBuffer(), WrittenCountOffset(0) {}
      ResourceId Buffer;
      uint64_t Offset;
      uint64_t Size;

      ResourceId WrittenCountBuffer;
      uint64_t WrittenCountOffset;
    };
    rdctype::array<Output> Outputs;
  } m_SO;

  struct Rasterizer
  {
    Rasterizer() : SampleMask(~0U) {}
    uint32_t SampleMask;

    struct Viewport
    {
      Viewport() : Width(0.0f), Height(0.0f), MinDepth(0.0f), MaxDepth(0.0f)
      {
        TopLeft[0] = 0.0f;
        TopLeft[1] = 0.0f;
      }
      Viewport(float TX, float TY, float W, float H, float MN, float MX)
          : Width(W), Height(H), MinDepth(MN), MaxDepth(MX)
      {
        TopLeft[0] = TX;
        TopLeft[1] = TY;
      }
      float TopLeft[2];
      float Width, Height;
      float MinDepth, MaxDepth;
    };
    rdctype::array<Viewport> Viewports;

    struct Scissor
    {
      Scissor() : left(0), top(0), right(0), bottom(0) {}
      Scissor(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
      int32_t left, top, right, bottom;
    };
    rdctype::array<Scissor> Scissors;

    struct RasterizerState
    {
      RasterizerState()
          : FillMode(eFill_Solid),
            CullMode(eCull_None),
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
      TriangleFillMode FillMode;
      TriangleCullMode CullMode;
      bool32 FrontCCW;
      int32_t DepthBias;
      float DepthBiasClamp;
      float SlopeScaledDepthBias;
      bool32 DepthClip;
      bool32 MultisampleEnable;
      bool32 AntialiasedLineEnable;
      uint32_t ForcedSampleCount;
      bool32 ConservativeRasterization;
    } m_State;
  } m_RS;

  struct OutputMerger
  {
    OutputMerger()
        : DepthReadOnly(false), StencilReadOnly(false), multiSampleCount(1), multiSampleQuality(0)
    {
    }
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

      struct StencilOp
      {
        rdctype::str FailOp;
        rdctype::str DepthFailOp;
        rdctype::str PassOp;
        rdctype::str Func;
      } m_FrontFace, m_BackFace;

      uint32_t StencilRef;
    } m_State;

    struct BlendState
    {
      BlendState() : AlphaToCoverage(false), IndependentBlend(false)
      {
        BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 0.0f;
      }

      bool32 AlphaToCoverage;
      bool32 IndependentBlend;

      struct RTBlend
      {
        RTBlend() : Enabled(false), LogicEnabled(false), WriteMask(0) {}
        struct BlendOp
        {
          rdctype::str Source;
          rdctype::str Destination;
          rdctype::str Operation;
        } m_Blend, m_AlphaBlend;

        rdctype::str LogicOp;

        bool32 Enabled;
        bool32 LogicEnabled;
        byte WriteMask;
      };
      rdctype::array<RTBlend> Blends;

      float BlendFactor[4];
    } m_BlendState;

    rdctype::array<ResourceView> RenderTargets;

    ResourceView DepthTarget;
    bool32 DepthReadOnly;
    bool32 StencilReadOnly;

    uint32_t multiSampleCount;
    uint32_t multiSampleQuality;
  } m_OM;

  struct ResourceData
  {
    ResourceId id;

    struct ResourceState
    {
      rdctype::str name;
    };
    rdctype::array<ResourceState> states;
  };
  rdctype::array<ResourceData> Resources;
};
