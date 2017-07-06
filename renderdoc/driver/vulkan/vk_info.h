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

#include "driver/shaders/spirv/spirv_common.h"
#include "vk_common.h"
#include "vk_manager.h"

struct VulkanCreationInfo;

struct DescSetLayout
{
  void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
            const VkDescriptorSetLayoutCreateInfo *pCreateInfo);

  void CreateBindingsArray(vector<DescriptorSetSlot *> &descBindings);

  struct Binding
  {
    // set reasonable defaults in the constructor as with sparse descriptor set layouts
    // some elements could be untouched. We set stageFlags to 0 so the UI ignores these
    // elements
    Binding()
        : descriptorType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
          descriptorCount(1),
          stageFlags(0),
          immutableSampler(NULL)
    {
    }
    // Copy the immutable sampler
    Binding(const Binding &b)
        : descriptorType(b.descriptorType),
          descriptorCount(b.descriptorCount),
          stageFlags(b.stageFlags),
          immutableSampler(NULL)
    {
      if(b.immutableSampler)
      {
        immutableSampler = new ResourceId[descriptorCount];
        memcpy(immutableSampler, b.immutableSampler, sizeof(ResourceId) * descriptorCount);
      }
    }
    ~Binding() { SAFE_DELETE_ARRAY(immutableSampler); }
    VkDescriptorType descriptorType;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    ResourceId *immutableSampler;
  };
  vector<Binding> bindings;

  uint32_t dynamicCount;

  bool operator==(const DescSetLayout &other) const;
  bool operator!=(const DescSetLayout &other) const { return !(*this == other); }
};

struct VulkanCreationInfo
{
  struct Pipeline
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkGraphicsPipelineCreateInfo *pCreateInfo);
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkComputePipelineCreateInfo *pCreateInfo);

    ResourceId layout;
    ResourceId renderpass;
    uint32_t subpass;

    // a variant of the pipeline that uses subpass 0, used for when we are replaying in isolation.
    // See loadRPs in the RenderPass info
    VkPipeline subpass0pipe;

    // VkGraphicsPipelineCreateInfo
    VkPipelineCreateFlags flags;

    // VkPipelineShaderStageCreateInfo
    struct Shader
    {
      Shader() : refl(NULL), mapping(NULL), patchData(NULL) {}
      ResourceId module;
      string entryPoint;
      ShaderReflection *refl;
      ShaderBindpointMapping *mapping;
      SPIRVPatchData *patchData;

      vector<byte> specdata;
      struct SpecInfo
      {
        uint32_t specID;
        byte *data;
        size_t size;
      };
      vector<SpecInfo> specialization;
    };
    Shader shaders[6];

    // VkPipelineVertexInputStateCreateInfo
    struct Binding
    {
      uint32_t vbufferBinding;
      uint32_t bytestride;
      bool perInstance;
    };
    vector<Binding> vertexBindings;

    struct Attribute
    {
      uint32_t location;
      uint32_t binding;
      VkFormat format;
      uint32_t byteoffset;
    };
    vector<Attribute> vertexAttrs;

    // VkPipelineInputAssemblyStateCreateInfo
    VkPrimitiveTopology topology;
    bool primitiveRestartEnable;

    // VkPipelineTessellationStateCreateInfo
    uint32_t patchControlPoints;

    // VkPipelineViewportStateCreateInfo
    uint32_t viewportCount;
    vector<VkViewport> viewports;
    vector<VkRect2D> scissors;

    // VkPipelineRasterizationStateCreateInfo
    bool depthClampEnable;
    bool rasterizerDiscardEnable;
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    bool depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth;

    // VkPipelineMultisampleStateCreateInfo
    VkSampleCountFlagBits rasterizationSamples;
    bool sampleShadingEnable;
    float minSampleShading;
    VkSampleMask sampleMask;
    bool alphaToCoverageEnable;
    bool alphaToOneEnable;

    // VkPipelineDepthStencilStateCreateInfo
    bool depthTestEnable;
    bool depthWriteEnable;
    VkCompareOp depthCompareOp;
    bool depthBoundsEnable;
    bool stencilTestEnable;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds;
    float maxDepthBounds;

    // VkPipelineColorBlendStateCreateInfo
    bool logicOpEnable;
    VkLogicOp logicOp;
    float blendConst[4];

    struct Attachment
    {
      bool blendEnable;

      struct BlendOp
      {
        VkBlendFactor Source;
        VkBlendFactor Destination;
        VkBlendOp Operation;
      } blend, alphaBlend;

      uint8_t channelWriteMask;
    };
    vector<Attachment> attachments;

    // VkPipelineDynamicStateCreateInfo
    bool dynamicStates[VK_DYNAMIC_STATE_RANGE_SIZE];
  };
  map<ResourceId, Pipeline> m_Pipeline;

  struct PipelineLayout
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkPipelineLayoutCreateInfo *pCreateInfo);

    vector<VkPushConstantRange> pushRanges;
    vector<ResourceId> descSetLayouts;
  };
  map<ResourceId, PipelineLayout> m_PipelineLayout;

  struct RenderPass
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkRenderPassCreateInfo *pCreateInfo);

    vector<VkAttachmentDescription> attachments;

    struct Subpass
    {
      // these are split apart since they layout is
      // rarely used but the indices are often used
      vector<uint32_t> inputAttachments;
      vector<uint32_t> colorAttachments;
      vector<uint32_t> resolveAttachments;
      int32_t depthstencilAttachment;

      vector<VkImageLayout> inputLayouts;
      vector<VkImageLayout> colorLayouts;
      VkImageLayout depthstencilLayout;
    };
    vector<Subpass> subpasses;

    // one for each subpass, as we preserve attachments
    // in the layout that the subpass uses
    vector<VkRenderPass> loadRPs;
  };
  map<ResourceId, RenderPass> m_RenderPass;

  struct Framebuffer
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkFramebufferCreateInfo *pCreateInfo);

    struct Attachment
    {
      ResourceId view;
      VkFormat format;
    };
    vector<Attachment> attachments;

    uint32_t width, height, layers;
  };
  map<ResourceId, Framebuffer> m_Framebuffer;

  struct Memory
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkMemoryAllocateInfo *pAllocInfo);

    uint64_t size;

    VkBuffer wholeMemBuf;
  };
  map<ResourceId, Memory> m_Memory;

  struct Buffer
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkBufferCreateInfo *pCreateInfo);

    VkBufferUsageFlags usage;
    uint64_t size;
  };
  map<ResourceId, Buffer> m_Buffer;

  struct BufferView
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkBufferViewCreateInfo *pCreateInfo);

    ResourceId buffer;
    uint64_t offset;
    uint64_t size;
  };
  map<ResourceId, BufferView> m_BufferView;

  struct Image
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkImageCreateInfo *pCreateInfo);

    VkImageView view;
    VkImageView stencilView;

    VkImageType type;
    VkFormat format;
    VkExtent3D extent;
    int arrayLayers, mipLevels;
    VkSampleCountFlagBits samples;

    bool cube;
    TextureCategory creationFlags;
  };
  map<ResourceId, Image> m_Image;

  struct Sampler
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkSamplerCreateInfo *pCreateInfo);

    VkFilter magFilter;
    VkFilter minFilter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode address[3];
    float mipLodBias;
    float maxAnisotropy;
    bool compareEnable;
    VkCompareOp compareOp;
    float minLod;
    float maxLod;
    VkBorderColor borderColor;
    bool unnormalizedCoordinates;
  };
  map<ResourceId, Sampler> m_Sampler;

  struct ImageView
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkImageViewCreateInfo *pCreateInfo);

    ResourceId image;
    VkFormat format;
    VkImageSubresourceRange range;
    TextureSwizzle swizzle[4];
  };
  map<ResourceId, ImageView> m_ImageView;

  struct ShaderModule
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkShaderModuleCreateInfo *pCreateInfo);

    SPVModule spirv;

    string unstrippedPath;

    struct Reflection
    {
      uint32_t stage;
      string entryPoint;
      string disassembly;
      ShaderReflection refl;
      ShaderBindpointMapping mapping;
      SPIRVPatchData patchData;
    };
    map<string, Reflection> m_Reflections;
  };
  map<ResourceId, ShaderModule> m_ShaderModule;

  map<ResourceId, string> m_Names;
  map<ResourceId, SwapchainInfo> m_SwapChain;
  map<ResourceId, DescSetLayout> m_DescSetLayout;
};
