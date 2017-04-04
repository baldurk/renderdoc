/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

namespace VKPipe
{
struct BindingElement
{
  BindingElement()
      : immutableSampler(false),
        customName(false),
        baseMip(0),
        baseLayer(0),
        offset(0),
        size(0),
        mipBias(0.0f),
        maxAniso(0.0f),
        compareEnable(false),
        minlod(0.0f),
        maxlod(0.0f),
        borderEnable(false),
        unnormalized(false)
  {
    swizzle[0] = TextureSwizzle::Red;
    swizzle[1] = TextureSwizzle::Green;
    swizzle[2] = TextureSwizzle::Blue;
    swizzle[3] = TextureSwizzle::Alpha;
  }

  ResourceId view;    // bufferview, imageview, attachmentview
  ResourceId res;     // buffer, image, attachment
  ResourceId sampler;
  bool32 immutableSampler;

  rdctype::str name;
  bool32 customName;

  // image views
  ResourceFormat viewfmt;
  TextureSwizzle swizzle[4];
  uint32_t baseMip;
  uint32_t baseLayer;
  uint32_t numMip;
  uint32_t numLayer;

  // buffers
  uint64_t offset;
  uint64_t size;

  // sampler info
  rdctype::str mag, min, mip;
  rdctype::str addrU, addrV, addrW;
  float mipBias;
  float maxAniso;
  bool32 compareEnable;
  rdctype::str comparison;
  float minlod, maxlod;
  bool32 borderEnable;
  rdctype::str border;
  bool32 unnormalized;
};

struct DescriptorBinding
{
  uint32_t descriptorCount;
  BindType type;
  ShaderStageMask stageFlags;

  // may only be one element if not an array
  rdctype::array<BindingElement> binds;
};

struct DescriptorSet
{
  ResourceId layout;
  ResourceId descset;

  rdctype::array<DescriptorBinding> bindings;
};

struct Pipeline
{
  Pipeline() : flags(0) {}
  ResourceId obj;
  uint32_t flags;

  rdctype::array<DescriptorSet> DescSets;
};

struct IB
{
  IB() : offs(0) {}
  ResourceId buf;
  uint64_t offs;
};

struct InputAssembly
{
  InputAssembly() : primitiveRestartEnable(false) {}
  bool32 primitiveRestartEnable;

  IB ibuffer;
};

struct VertexAttribute
{
  VertexAttribute() : location(0), binding(0), format(), byteoffset(0) {}
  uint32_t location;
  uint32_t binding;
  ResourceFormat format;
  uint32_t byteoffset;
};

struct VertexBinding
{
  VertexBinding() : vbufferBinding(0), bytestride(0), perInstance(false) {}
  uint32_t vbufferBinding;
  uint32_t bytestride;
  bool32 perInstance;
};

struct VB
{
  VB() : offset(0) {}
  ResourceId buffer;
  uint64_t offset;
};

struct VertexInput
{
  rdctype::array<VertexAttribute> attrs;
  rdctype::array<VertexBinding> binds;
  rdctype::array<VB> vbuffers;
};

struct SpecInfo
{
  SpecInfo() : specID(0) {}
  uint32_t specID;
  rdctype::array<byte> data;
};

struct Shader
{
  Shader() : Object(), customName(false), ShaderDetails(NULL), stage(ShaderStage::Vertex) {}
  ResourceId Object;
  rdctype::str entryPoint;

  rdctype::str name;
  bool32 customName;
  ShaderReflection *ShaderDetails;

  // this is no longer dynamic, like GL, but it's also not trivial, like D3D11.
  // this contains the mapping between the shader objects in the reflection data
  // and the descriptor set and binding that they use
  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage;

  rdctype::array<SpecInfo> specialization;
};

struct Tessellation
{
  Tessellation() : numControlPoints(0) {}
  uint32_t numControlPoints;
};

struct Viewport
{
  Viewport() : x(0), y(0), width(0), height(0), minDepth(0), maxDepth(0) {}
  float x, y, width, height, minDepth, maxDepth;
};

struct Scissor
{
  Scissor() : x(0), y(0), width(0), height(0) {}
  int32_t x, y, width, height;
};

struct ViewportScissor
{
  Viewport vp;
  Scissor scissor;
};

struct ViewState
{
  rdctype::array<ViewportScissor> viewportScissors;
};

struct Raster
{
  Raster()
      : depthClampEnable(false),
        rasterizerDiscardEnable(false),
        FrontCCW(false),
        fillMode(FillMode::Solid),
        cullMode(CullMode::NoCull),
        depthBias(0),
        depthBiasClamp(0),
        slopeScaledDepthBias(0),
        lineWidth(0)
  {
  }

  bool32 depthClampEnable, rasterizerDiscardEnable, FrontCCW;
  FillMode fillMode;
  CullMode cullMode;

  // dynamic
  float depthBias, depthBiasClamp, slopeScaledDepthBias, lineWidth;
};

struct MultiSample
{
  MultiSample() : rasterSamples(0), sampleShadingEnable(false), minSampleShading(0), sampleMask(~0U)
  {
  }
  uint32_t rasterSamples;
  bool32 sampleShadingEnable;
  float minSampleShading;
  uint32_t sampleMask;
};

struct BlendOp
{
  rdctype::str Source;
  rdctype::str Destination;
  rdctype::str Operation;
};

struct Blend
{
  Blend() : blendEnable(false), writeMask(0) {}
  bool32 blendEnable;

  BlendOp blend, alphaBlend;

  uint8_t writeMask;
};

struct ColorBlend
{
  ColorBlend() : alphaToCoverageEnable(false), alphaToOneEnable(false), logicOpEnable(false)
  {
    blendConst[0] = blendConst[1] = blendConst[2] = blendConst[3] = 0.0f;
  }

  bool32 alphaToCoverageEnable, alphaToOneEnable, logicOpEnable;
  rdctype::str logicOp;

  rdctype::array<Blend> attachments;

  // dynamic
  float blendConst[4];
};

struct StencilOp
{
  StencilOp() : ref(0), compareMask(0xff), writeMask(0xff) {}
  rdctype::str failOp;
  rdctype::str depthFailOp;
  rdctype::str passOp;
  rdctype::str func;

  // dynamic
  uint32_t ref, compareMask, writeMask;
};

struct DepthStencil
{
  DepthStencil()
      : depthTestEnable(false),
        depthWriteEnable(false),
        depthBoundsEnable(false),
        stencilTestEnable(false),
        minDepthBounds(0),
        maxDepthBounds(0)
  {
  }

  bool32 depthTestEnable, depthWriteEnable, depthBoundsEnable;
  rdctype::str depthCompareOp;

  bool32 stencilTestEnable;

  StencilOp front, back;

  // dynamic
  float minDepthBounds, maxDepthBounds;
};

struct RenderPass
{
  RenderPass() : depthstencilAttachment(-1) {}
  ResourceId obj;
  // VKTODOMED renderpass and subpass information here

  rdctype::array<uint32_t> inputAttachments;
  rdctype::array<uint32_t> colorAttachments;
  int32_t depthstencilAttachment;
};

struct Attachment
{
  Attachment() : baseMip(0), baseLayer(0), numMip(1), numLayer(1)
  {
    swizzle[0] = TextureSwizzle::Red;
    swizzle[1] = TextureSwizzle::Green;
    swizzle[2] = TextureSwizzle::Blue;
    swizzle[3] = TextureSwizzle::Alpha;
  }
  ResourceId view;
  ResourceId img;

  ResourceFormat viewfmt;
  TextureSwizzle swizzle[4];
  uint32_t baseMip;
  uint32_t baseLayer;
  uint32_t numMip;
  uint32_t numLayer;
};

struct Framebuffer
{
  Framebuffer() : width(0), height(0), layers(0) {}
  ResourceId obj;

  rdctype::array<Attachment> attachments;

  uint32_t width, height, layers;
};

struct RenderArea
{
  RenderArea() : x(0), y(0), width(0), height(0) {}
  int32_t x, y, width, height;
};

struct CurrentPass
{
  RenderPass renderpass;
  Framebuffer framebuffer;
  RenderArea renderArea;
};

struct ImageLayout
{
  uint32_t baseMip;
  uint32_t baseLayer;
  uint32_t numMip;
  uint32_t numLayer;
  rdctype::str name;
};

struct ImageData
{
  ResourceId image;

  rdctype::array<ImageLayout> layouts;
};

struct State
{
  Pipeline compute, graphics;

  InputAssembly IA;
  VertexInput VI;

  Shader m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;

  Tessellation Tess;

  ViewState VP;
  Raster RS;

  MultiSample MSAA;
  ColorBlend CB;
  DepthStencil DS;

  CurrentPass Pass;

  rdctype::array<ImageData> images;
};

};    // namespace VKPipe

DECLARE_REFLECTION_STRUCT(VKPipe::BindingElement);
DECLARE_REFLECTION_STRUCT(VKPipe::DescriptorBinding);
DECLARE_REFLECTION_STRUCT(VKPipe::DescriptorSet);
DECLARE_REFLECTION_STRUCT(VKPipe::Pipeline);
DECLARE_REFLECTION_STRUCT(VKPipe::IB);
DECLARE_REFLECTION_STRUCT(VKPipe::InputAssembly);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexBinding);
DECLARE_REFLECTION_STRUCT(VKPipe::VB);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexInput);
DECLARE_REFLECTION_STRUCT(VKPipe::SpecInfo);
DECLARE_REFLECTION_STRUCT(VKPipe::Shader);
DECLARE_REFLECTION_STRUCT(VKPipe::Tessellation);
DECLARE_REFLECTION_STRUCT(VKPipe::Viewport);
DECLARE_REFLECTION_STRUCT(VKPipe::Scissor);
DECLARE_REFLECTION_STRUCT(VKPipe::ViewportScissor);
DECLARE_REFLECTION_STRUCT(VKPipe::ViewState);
DECLARE_REFLECTION_STRUCT(VKPipe::Raster);
DECLARE_REFLECTION_STRUCT(VKPipe::MultiSample);
DECLARE_REFLECTION_STRUCT(VKPipe::BlendOp);
DECLARE_REFLECTION_STRUCT(VKPipe::Blend);
DECLARE_REFLECTION_STRUCT(VKPipe::ColorBlend);
DECLARE_REFLECTION_STRUCT(VKPipe::StencilOp);
DECLARE_REFLECTION_STRUCT(VKPipe::DepthStencil);
DECLARE_REFLECTION_STRUCT(VKPipe::RenderPass);
DECLARE_REFLECTION_STRUCT(VKPipe::Attachment);
DECLARE_REFLECTION_STRUCT(VKPipe::Framebuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::RenderArea);
DECLARE_REFLECTION_STRUCT(VKPipe::CurrentPass);
DECLARE_REFLECTION_STRUCT(VKPipe::ImageLayout);
DECLARE_REFLECTION_STRUCT(VKPipe::ImageData);
DECLARE_REFLECTION_STRUCT(VKPipe::State);
