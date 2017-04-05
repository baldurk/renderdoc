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
  ResourceId view;    // bufferview, imageview, attachmentview
  ResourceId res;     // buffer, image, attachment
  ResourceId sampler;
  bool32 immutableSampler = false;

  rdctype::str name;
  bool32 customName = false;

  // image views
  ResourceFormat viewfmt;
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  uint32_t baseMip = 0;
  uint32_t baseLayer = 0;
  uint32_t numMip = 0;
  uint32_t numLayer = 0;

  // buffers
  uint64_t offset = 0;
  uint64_t size = 0;

  // sampler info
  rdctype::str mag, min, mip;
  rdctype::str addrU, addrV, addrW;
  float mipBias = 0.0f;
  float maxAniso = 0.0f;
  bool32 compareEnable = false;
  rdctype::str comparison;
  float minlod = 0.0f;
  float maxlod = 0.0f;
  bool32 borderEnable = false;
  rdctype::str border;
  bool32 unnormalized = false;
};

struct DescriptorBinding
{
  uint32_t descriptorCount = 0;
  BindType type = BindType::Unknown;
  ShaderStageMask stageFlags = ShaderStageMask::Unknown;

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
  ResourceId obj;
  uint32_t flags = 0;

  rdctype::array<DescriptorSet> DescSets;
};

struct IB
{
  ResourceId buf;
  uint64_t offs = 0;
};

struct InputAssembly
{
  bool32 primitiveRestartEnable = false;

  IB ibuffer;
};

struct VertexAttribute
{
  uint32_t location = 0;
  uint32_t binding = 0;
  ResourceFormat format;
  uint32_t byteoffset = 0;
};

struct VertexBinding
{
  uint32_t vbufferBinding = 0;
  uint32_t bytestride = 0;
  bool32 perInstance = false;
};

struct VB
{
  ResourceId buffer;
  uint64_t offset = 0;
};

struct VertexInput
{
  rdctype::array<VertexAttribute> attrs;
  rdctype::array<VertexBinding> binds;
  rdctype::array<VB> vbuffers;
};

struct SpecInfo
{
  uint32_t specID = 0;
  rdctype::array<byte> data;
};

struct Shader
{
  ResourceId Object;
  rdctype::str entryPoint;

  rdctype::str name;
  bool32 customName = false;
  ShaderReflection *ShaderDetails = NULL;

  // this is no longer dynamic, like GL, but it's also not trivial, like D3D11.
  // this contains the mapping between the shader objects in the reflection data
  // and the descriptor set and binding that they use
  ShaderBindpointMapping BindpointMapping;

  ShaderStage stage = ShaderStage::Vertex;

  rdctype::array<SpecInfo> specialization;
};

struct Tessellation
{
  uint32_t numControlPoints = 0;
};

struct Viewport
{
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float minDepth = 0.0f;
  float maxDepth = 0.0f;
};

struct Scissor
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
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
  bool32 depthClampEnable = false;
  bool32 rasterizerDiscardEnable = false;
  bool32 FrontCCW = false;
  FillMode fillMode = FillMode::Solid;
  CullMode cullMode = CullMode::NoCull;

  // dynamic
  float depthBias = 0.0f;
  float depthBiasClamp = 0.0f;
  float slopeScaledDepthBias = 0.0f;
  float lineWidth = 0.0f;
};

struct MultiSample
{
  uint32_t rasterSamples = 0;
  bool32 sampleShadingEnable = false;
  float minSampleShading = 0.0f;
  uint32_t sampleMask = 0;
};

struct BlendOp
{
  rdctype::str Source;
  rdctype::str Destination;
  rdctype::str Operation;
};

struct Blend
{
  bool32 blendEnable = false;

  BlendOp blend, alphaBlend;

  uint8_t writeMask = 0;
};

struct ColorBlend
{
  bool32 alphaToCoverageEnable = false;
  bool32 alphaToOneEnable = false;
  bool32 logicOpEnable = false;
  rdctype::str logicOp;

  rdctype::array<Blend> attachments;

  // dynamic
  float blendConst[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct StencilOp
{
  rdctype::str failOp;
  rdctype::str depthFailOp;
  rdctype::str passOp;
  rdctype::str func;

  // dynamic
  uint32_t ref = 0;
  uint32_t compareMask = 0xff;
  uint32_t writeMask = 0xff;
};

struct DepthStencil
{
  bool32 depthTestEnable = false;
  bool32 depthWriteEnable = false;
  bool32 depthBoundsEnable = false;
  rdctype::str depthCompareOp;

  bool32 stencilTestEnable = false;

  StencilOp front, back;

  // dynamic
  float minDepthBounds = 0.0f;
  float maxDepthBounds = 0.0f;
};

struct RenderPass
{
  ResourceId obj;
  // VKTODOMED renderpass and subpass information here

  rdctype::array<uint32_t> inputAttachments;
  rdctype::array<uint32_t> colorAttachments;
  int32_t depthstencilAttachment = -1;
};

struct Attachment
{
  ResourceId view;
  ResourceId img;

  ResourceFormat viewfmt;
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  uint32_t baseMip = 0;
  uint32_t baseLayer = 0;
  uint32_t numMip = 1;
  uint32_t numLayer = 1;
};

struct Framebuffer
{
  ResourceId obj;

  rdctype::array<Attachment> attachments;

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t layers = 0;
};

struct RenderArea
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
};

struct CurrentPass
{
  RenderPass renderpass;
  Framebuffer framebuffer;
  RenderArea renderArea;
};

struct ImageLayout
{
  uint32_t baseMip = 0;
  uint32_t baseLayer = 0;
  uint32_t numMip = 1;
  uint32_t numLayer = 1;
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
