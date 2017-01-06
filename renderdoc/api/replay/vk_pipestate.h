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

struct VulkanPipelineState
{
  struct Pipeline
  {
    Pipeline() : flags(0) {}
    ResourceId obj;
    uint32_t flags;

    struct DescriptorSet
    {
      ResourceId layout;
      ResourceId descset;

      struct DescriptorBinding
      {
        uint32_t descriptorCount;
        ShaderBindType type;
        ShaderStageBits stageFlags;

        struct BindingElement
        {
          BindingElement()
              : immutableSampler(false),
                customSamplerName(false),
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
            swizzle[0] = eSwizzle_Red;
            swizzle[1] = eSwizzle_Green;
            swizzle[2] = eSwizzle_Blue;
            swizzle[3] = eSwizzle_Alpha;
          }

          ResourceId view;    // bufferview, imageview, attachmentview
          ResourceId res;     // buffer, image, attachment
          ResourceId sampler;
          bool32 immutableSampler;

          rdctype::str SamplerName;
          bool32 customSamplerName;

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

        // may only be one element if not an array
        rdctype::array<BindingElement> binds;
      };
      rdctype::array<DescriptorBinding> bindings;
    };

    rdctype::array<DescriptorSet> DescSets;
  } compute, graphics;

  struct InputAssembly
  {
    InputAssembly() : primitiveRestartEnable(false) {}
    bool32 primitiveRestartEnable;

    struct IndexBuffer
    {
      IndexBuffer() : offs(0) {}
      ResourceId buf;
      uint64_t offs;
    } ibuffer;
  } IA;

  struct VertexInput
  {
    struct Attribute
    {
      Attribute() : location(0), binding(0), format(), byteoffset(0) {}
      uint32_t location;
      uint32_t binding;
      ResourceFormat format;
      uint32_t byteoffset;
    };
    rdctype::array<Attribute> attrs;

    struct Binding
    {
      Binding() : vbufferBinding(0), bytestride(0), perInstance(false) {}
      uint32_t vbufferBinding;
      uint32_t bytestride;
      bool32 perInstance;
    };
    rdctype::array<Binding> binds;

    struct VertexBuffer
    {
      VertexBuffer() : offset(0) {}
      ResourceId buffer;
      uint64_t offset;
    };
    rdctype::array<VertexBuffer> vbuffers;
  } VI;

  struct ShaderStage
  {
    ShaderStage() : Shader(), customName(false), ShaderDetails(NULL), stage(eShaderStage_Vertex) {}
    ResourceId Shader;
    rdctype::str entryPoint;

    rdctype::str ShaderName;
    bool32 customName;
    ShaderReflection *ShaderDetails;

    // this is no longer dynamic, like GL, but it's also not trivial, like D3D11.
    // this contains the mapping between the shader objects in the reflection data
    // and the descriptor set and binding that they use
    ShaderBindpointMapping BindpointMapping;

    ShaderStageType stage;

    struct SpecInfo
    {
      SpecInfo() : specID(0) {}
      uint32_t specID;
      rdctype::array<byte> data;
    };
    rdctype::array<SpecInfo> specialization;
  } VS, TCS, TES, GS, FS, CS;

  struct Tessellation
  {
    Tessellation() : numControlPoints(0) {}
    uint32_t numControlPoints;
  } Tess;

  struct ViewState
  {
    struct ViewportScissor
    {
      struct Viewport
      {
        Viewport() : x(0), y(0), width(0), height(0), minDepth(0), maxDepth(0) {}
        float x, y, width, height, minDepth, maxDepth;
      } vp;

      struct Scissor
      {
        Scissor() : x(0), y(0), width(0), height(0) {}
        int32_t x, y, width, height;
      } scissor;
    };

    rdctype::array<ViewportScissor> viewportScissors;
  } VP;

  struct Raster
  {
    Raster()
        : depthClampEnable(false),
          rasterizerDiscardEnable(false),
          FrontCCW(false),
          FillMode(eFill_Solid),
          CullMode(eCull_None),
          depthBias(0),
          depthBiasClamp(0),
          slopeScaledDepthBias(0),
          lineWidth(0)
    {
    }

    bool32 depthClampEnable, rasterizerDiscardEnable, FrontCCW;
    TriangleFillMode FillMode;
    TriangleCullMode CullMode;

    // dynamic
    float depthBias, depthBiasClamp, slopeScaledDepthBias, lineWidth;
  } RS;

  struct MultiSample
  {
    MultiSample()
        : rasterSamples(0), sampleShadingEnable(false), minSampleShading(0), sampleMask(~0U)
    {
    }
    uint32_t rasterSamples;
    bool32 sampleShadingEnable;
    float minSampleShading;
    uint32_t sampleMask;
  } MSAA;

  struct ColorBlend
  {
    ColorBlend() : alphaToCoverageEnable(false), alphaToOneEnable(false), logicOpEnable(false)
    {
      blendConst[0] = blendConst[1] = blendConst[2] = blendConst[3] = 0.0f;
    }

    bool32 alphaToCoverageEnable, alphaToOneEnable, logicOpEnable;
    rdctype::str logicOp;

    struct Attachment
    {
      Attachment() : blendEnable(false), writeMask(0) {}
      bool32 blendEnable;

      struct BlendOp
      {
        rdctype::str Source;
        rdctype::str Destination;
        rdctype::str Operation;
      } blend, alphaBlend;

      uint8_t writeMask;
    };
    rdctype::array<Attachment> attachments;

    // dynamic
    float blendConst[4];
  } CB;

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
    struct StencilOp
    {
      StencilOp() : ref(0), compareMask(0xff), writeMask(0xff) {}
      rdctype::str failOp;
      rdctype::str depthFailOp;
      rdctype::str passOp;
      rdctype::str func;

      // dynamic
      uint32_t ref, compareMask, writeMask;
    } front, back;

    // dynamic
    float minDepthBounds, maxDepthBounds;
  } DS;

  struct CurrentPass
  {
    struct RenderPass
    {
      RenderPass() : depthstencilAttachment(-1) {}
      ResourceId obj;
      // VKTODOMED renderpass and subpass information here

      rdctype::array<uint32_t> inputAttachments;
      rdctype::array<uint32_t> colorAttachments;
      int32_t depthstencilAttachment;
    } renderpass;

    struct Framebuffer
    {
      Framebuffer() : width(0), height(0), layers(0) {}
      ResourceId obj;

      struct Attachment
      {
        Attachment() : baseMip(0), baseLayer(0), numMip(1), numLayer(1)
        {
          swizzle[0] = eSwizzle_Red;
          swizzle[1] = eSwizzle_Green;
          swizzle[2] = eSwizzle_Blue;
          swizzle[3] = eSwizzle_Alpha;
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
      rdctype::array<Attachment> attachments;

      uint32_t width, height, layers;
    } framebuffer;

    struct RenderArea
    {
      RenderArea() : x(0), y(0), width(0), height(0) {}
      int32_t x, y, width, height;
    } renderArea;
  } Pass;

  struct ImageData
  {
    ResourceId image;

    struct ImageLayout
    {
      uint32_t baseMip;
      uint32_t baseLayer;
      uint32_t numMip;
      uint32_t numLayer;
      rdctype::str name;
    };
    rdctype::array<ImageLayout> layouts;
  };
  rdctype::array<ImageData> images;
};
