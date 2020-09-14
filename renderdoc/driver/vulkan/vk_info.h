/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <unordered_map>
#include "driver/shaders/spirv/spirv_reflect.h"
#include "vk_common.h"
#include "vk_manager.h"

struct VulkanCreationInfo;

// linearised version of VkDynamicState
enum VulkanDynamicStateIndex
{
  VkDynamicViewport,
  VkDynamicScissor,
  VkDynamicLineWidth,
  VkDynamicDepthBias,
  VkDynamicBlendConstants,
  VkDynamicDepthBounds,
  VkDynamicStencilCompareMask,
  VkDynamicStencilWriteMask,
  VkDynamicStencilReference,
  VkDynamicViewportWScalingNV,
  VkDynamicDiscardRectangleEXT,
  VkDynamicSampleLocationsEXT,
  VkDynamicViewportShadingRatePaletteNV,
  VkDynamicViewportCoarseSampleOrderNV,
  VkDynamicExclusiveScissorNV,
  VkDynamicLineStippleEXT,
  VkDynamicCullModeEXT,
  VkDynamicFrontFaceEXT,
  VkDynamicPrimitiveTopologyEXT,
  VkDynamicViewportCountEXT,
  VkDynamicScissorCountEXT,
  VkDynamicVertexInputBindingStrideEXT,
  VkDynamicDepthTestEnableEXT,
  VkDynamicDepthWriteEnableEXT,
  VkDynamicDepthCompareOpEXT,
  VkDynamicDepthBoundsTestEnableEXT,
  VkDynamicStencilTestEnableEXT,
  VkDynamicStencilOpEXT,
  VkDynamicCount,
};

VkDynamicState ConvertDynamicState(VulkanDynamicStateIndex idx);
VulkanDynamicStateIndex ConvertDynamicState(VkDynamicState state);

struct DescSetLayout
{
  void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
            const VkDescriptorSetLayoutCreateInfo *pCreateInfo);

  void CreateBindingsArray(BindingStorage &bindingStorage, uint32_t variableAllocSize) const;
  void UpdateBindingsArray(const DescSetLayout &prevLayout, BindingStorage &bindingStorage) const;

  struct Binding
  {
    // set reasonable defaults in the constructor as with sparse descriptor set layouts
    // some elements could be untouched. We set stageFlags to 0 so the UI ignores these
    // elements
    Binding()
        : descriptorType(VK_DESCRIPTOR_TYPE_MAX_ENUM),
          elemOffset(0),
          descriptorCount(0),
          stageFlags(0),
          variableSize(0),
          immutableSampler(NULL)
    {
    }
    // move the immutable sampler
    Binding(Binding &&b)
        : descriptorType(b.descriptorType),
          elemOffset(b.elemOffset),
          descriptorCount(b.descriptorCount),
          stageFlags(b.stageFlags),
          variableSize(b.variableSize),
          immutableSampler(b.immutableSampler)
    {
      b.immutableSampler = NULL;
    }
    // Copy the immutable sampler
    Binding(const Binding &b)
        : descriptorType(b.descriptorType),
          elemOffset(b.elemOffset),
          descriptorCount(b.descriptorCount),
          stageFlags(b.stageFlags),
          variableSize(b.variableSize),
          immutableSampler(NULL)
    {
      if(b.immutableSampler)
      {
        immutableSampler = new ResourceId[descriptorCount];
        memcpy(immutableSampler, b.immutableSampler, sizeof(ResourceId) * descriptorCount);
      }
    }
    const Binding &operator=(const Binding &b)
    {
      if(this == &b)
        return *this;

      descriptorType = b.descriptorType;
      elemOffset = b.elemOffset;
      descriptorCount = b.descriptorCount;
      stageFlags = b.stageFlags;
      variableSize = b.variableSize;
      SAFE_DELETE_ARRAY(immutableSampler);
      if(b.immutableSampler)
      {
        immutableSampler = new ResourceId[descriptorCount];
        memcpy(immutableSampler, b.immutableSampler, sizeof(ResourceId) * descriptorCount);
      }
      return *this;
    }
    ~Binding() { SAFE_DELETE_ARRAY(immutableSampler); }
    VkDescriptorType descriptorType;
    uint32_t elemOffset;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags : 31;
    uint32_t variableSize : 1;
    ResourceId *immutableSampler;
  };
  rdcarray<Binding> bindings;

  uint32_t totalElems;
  uint32_t dynamicCount;
  VkDescriptorSetLayoutCreateFlags flags;

  uint32_t inlineCount;
  uint32_t inlineByteSize;

  bool operator==(const DescSetLayout &other) const;
  bool operator!=(const DescSetLayout &other) const { return !(*this == other); }
};

struct DescUpdateTemplateApplication
{
  rdcarray<VkDescriptorBufferInfo> bufInfo;
  rdcarray<VkDescriptorImageInfo> imgInfo;
  rdcarray<VkBufferView> bufView;
  rdcarray<VkWriteDescriptorSetInlineUniformBlockEXT> inlineUniform;
  bytebuf inlineData;

  rdcarray<VkWriteDescriptorSet> writes;
};

struct DescUpdateTemplate
{
  void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
            const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo);

  void Apply(const void *pData, DescUpdateTemplateApplication &application);

  DescSetLayout layout;

  VkPipelineBindPoint bindPoint;

  size_t unwrapByteSize;

  uint32_t texelBufferViewCount;
  uint32_t bufferInfoCount;
  uint32_t imageInfoCount;
  uint32_t inlineInfoCount;
  uint32_t inlineByteSize;

  rdcarray<VkDescriptorUpdateTemplateEntry> updates;
};

struct VulkanCreationInfo
{
  struct ShaderModuleReflectionKey
  {
    ShaderModuleReflectionKey(const rdcstr &e, ResourceId p) : entryPoint(e), specialisingPipe(p) {}
    bool operator<(const ShaderModuleReflectionKey &o) const
    {
      if(entryPoint != o.entryPoint)
        return entryPoint < o.entryPoint;

      return specialisingPipe < o.specialisingPipe;
    }

    // name of the entry point
    rdcstr entryPoint;
    // ID of the pipeline ONLY if it contains specialisation constant data
    ResourceId specialisingPipe;
  };

  struct ShaderModuleReflection
  {
    uint32_t stageIndex;
    rdcstr entryPoint;
    rdcstr disassembly;
    ShaderReflection refl;
    ShaderBindpointMapping mapping;
    SPIRVPatchData patchData;
    std::map<size_t, uint32_t> instructionLines;

    void Init(VulkanResourceManager *resourceMan, ResourceId id, const rdcspv::Reflector &spv,
              const rdcstr &entry, VkShaderStageFlagBits stage,
              const rdcarray<SpecConstant> &specInfo);

    void PopulateDisassembly(const rdcspv::Reflector &spirv);
  };

  struct Pipeline
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, ResourceId id,
              const VkGraphicsPipelineCreateInfo *pCreateInfo);
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, ResourceId id,
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
      rdcstr entryPoint;
      ShaderReflection *refl;
      ShaderBindpointMapping *mapping;
      SPIRVPatchData *patchData;

      rdcarray<SpecConstant> specialization;
    };
    Shader shaders[6];

    // VkPipelineVertexInputStateCreateInfo
    struct Binding
    {
      uint32_t vbufferBinding;
      uint32_t bytestride;
      bool perInstance;

      // VkVertexInputBindingDivisorDescriptionEXT
      uint32_t instanceDivisor;
    };
    rdcarray<Binding> vertexBindings;

    struct Attribute
    {
      uint32_t location;
      uint32_t binding;
      VkFormat format;
      uint32_t byteoffset;
    };
    rdcarray<Attribute> vertexAttrs;

    // VkPipelineInputAssemblyStateCreateInfo
    VkPrimitiveTopology topology;
    bool primitiveRestartEnable;

    // VkPipelineTessellationStateCreateInfo
    uint32_t patchControlPoints;

    // VkPipelineTessellationDomainOriginStateCreateInfo
    VkTessellationDomainOrigin tessellationDomainOrigin;

    // VkPipelineViewportStateCreateInfo
    uint32_t viewportCount;
    rdcarray<VkViewport> viewports;
    rdcarray<VkRect2D> scissors;

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

    // VkPipelineRasterizationStateStreamCreateInfoEXT
    uint32_t rasterizationStream;

    // VkPipelineRasterizationStateStreamCreateInfoEXT
    bool depthClipEnable;

    // VkPipelineRasterizationConservativeStateCreateInfoEXT
    VkConservativeRasterizationModeEXT conservativeRasterizationMode;
    float extraPrimitiveOverestimationSize;

    // VkPipelineRasterizationLineStateCreateInfoEXT
    VkLineRasterizationModeEXT lineRasterMode;
    bool stippleEnabled;
    uint32_t stippleFactor;
    uint16_t stipplePattern;

    // VkPipelineMultisampleStateCreateInfo
    VkSampleCountFlagBits rasterizationSamples;
    bool sampleShadingEnable;
    float minSampleShading;
    VkSampleMask sampleMask;
    bool alphaToCoverageEnable;
    bool alphaToOneEnable;

    // VkPipelineSampleLocationsStateCreateInfoEXT
    struct
    {
      bool enabled;
      VkExtent2D gridSize;
      rdcarray<VkSampleLocationEXT> locations;
    } sampleLocations;

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
    rdcarray<Attachment> attachments;

    // VkPipelineDynamicStateCreateInfo
    bool dynamicStates[VkDynamicCount];

    // VkPipelineDiscardRectangleStateCreateInfoEXT
    rdcarray<VkRect2D> discardRectangles;
    VkDiscardRectangleModeEXT discardMode;
  };
  std::unordered_map<ResourceId, Pipeline> m_Pipeline;

  struct PipelineLayout
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkPipelineLayoutCreateInfo *pCreateInfo);

    rdcarray<VkPushConstantRange> pushRanges;
    rdcarray<ResourceId> descSetLayouts;
  };
  std::unordered_map<ResourceId, PipelineLayout> m_PipelineLayout;

  struct RenderPass
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkRenderPassCreateInfo *pCreateInfo);
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkRenderPassCreateInfo2 *pCreateInfo);

    struct Attachment
    {
      bool used;
      VkAttachmentDescriptionFlags flags;
      VkFormat format;
      VkSampleCountFlagBits samples;
      VkAttachmentLoadOp loadOp;
      VkAttachmentStoreOp storeOp;
      VkAttachmentLoadOp stencilLoadOp;
      VkAttachmentStoreOp stencilStoreOp;
      VkImageLayout initialLayout;
      VkImageLayout finalLayout;
      VkImageLayout stencilInitialLayout;
      VkImageLayout stencilFinalLayout;
    };

    rdcarray<Attachment> attachments;

    struct Subpass
    {
      // these are split apart since they layout is
      // rarely used but the indices are often used
      rdcarray<uint32_t> inputAttachments;
      rdcarray<uint32_t> colorAttachments;
      rdcarray<uint32_t> resolveAttachments;
      int32_t depthstencilAttachment;
      int32_t fragmentDensityAttachment;

      rdcarray<VkImageLayout> inputLayouts;
      rdcarray<VkImageLayout> inputStencilLayouts;
      rdcarray<VkImageLayout> colorLayouts;
      VkImageLayout depthLayout;
      VkImageLayout stencilLayout;
      VkImageLayout fragmentDensityLayout;

      rdcarray<uint32_t> multiviews;
    };
    rdcarray<Subpass> subpasses;

    // one for each subpass, as we preserve attachments
    // in the layout that the subpass uses
    rdcarray<VkRenderPass> loadRPs;
  };
  std::unordered_map<ResourceId, RenderPass> m_RenderPass;

  struct Framebuffer
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkFramebufferCreateInfo *pCreateInfo);

    struct Attachment
    {
      ResourceId createdView;
      bool hasStencil;
    };
    rdcarray<Attachment> attachments;
    bool imageless;

    uint32_t width, height, layers;

    // See above in loadRPs - we need to duplicate and make framebuffer equivalents for each
    rdcarray<VkFramebuffer> loadFBs;
  };
  std::unordered_map<ResourceId, Framebuffer> m_Framebuffer;

  struct Memory
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkMemoryAllocateInfo *pAllocInfo);

    uint32_t memoryTypeIndex;
    uint64_t size;

    VkBuffer wholeMemBuf;

    enum MemoryBinding
    {
      None = 0x0,
      Linear = 0x1,
      Tiled = 0x2,
      LinearAndTiled = 0x3,
    };

    Intervals<MemoryBinding> bindings;

    void BindMemory(uint64_t offs, uint64_t sz, MemoryBinding b)
    {
      bindings.update(offs, offs + sz, b,
                      [](MemoryBinding a, MemoryBinding b) { return MemoryBinding(a | b); });
    }

    void SimplifyBindings();
  };
  std::unordered_map<ResourceId, Memory> m_Memory;

  struct Buffer
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkBufferCreateInfo *pCreateInfo);

    VkBufferUsageFlags usage;
    uint64_t size;
    uint64_t gpuAddress;
    bool external;
  };
  std::unordered_map<ResourceId, Buffer> m_Buffer;

  struct BufferView
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkBufferViewCreateInfo *pCreateInfo);

    ResourceId buffer;
    VkFormat format;
    uint64_t offset;
    uint64_t size;
  };
  std::unordered_map<ResourceId, BufferView> m_BufferView;

  struct Image
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkImageCreateInfo *pCreateInfo);

    VkImageType type;
    VkFormat format;
    VkExtent3D extent;
    uint32_t arrayLayers, mipLevels;
    VkSampleCountFlagBits samples;

    bool linear;
    bool external;
    bool cube;
    TextureCategory creationFlags;
  };
  std::unordered_map<ResourceId, Image> m_Image;

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

    // VkSamplerReductionModeCreateInfo
    VkSamplerReductionMode reductionMode;

    // VkSamplerYcbcrConversionInfo
    ResourceId ycbcr;

    // VkSamplerCustomBorderColorCreateInfoEXT
    bool customBorder;
    VkClearColorValue customBorderColor;
    VkFormat customBorderFormat;
  };
  std::unordered_map<ResourceId, Sampler> m_Sampler;

  struct YCbCrSampler
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkSamplerYcbcrConversionCreateInfo *pCreateInfo);

    YcbcrConversion ycbcrModel;
    YcbcrRange ycbcrRange;
    VkComponentMapping componentMapping;
    ChromaSampleLocation xChromaOffset;
    ChromaSampleLocation yChromaOffset;
    FilterMode chromaFilter;
    bool forceExplicitReconstruction;
  };
  std::unordered_map<ResourceId, YCbCrSampler> m_YCbCrSampler;

  struct ImageView
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkImageViewCreateInfo *pCreateInfo);

    ResourceId image;
    VkImageViewType viewType;
    VkFormat format;
    VkImageSubresourceRange range;
    VkComponentMapping componentMapping;
  };
  std::unordered_map<ResourceId, ImageView> m_ImageView;

  struct ShaderModule
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkShaderModuleCreateInfo *pCreateInfo);

    ShaderModuleReflection &GetReflection(const rdcstr &entry, ResourceId pipe)
    {
      // look for one from this pipeline specifically, if it was specialised
      auto it = m_Reflections.find({entry, pipe});
      if(it != m_Reflections.end())
        return it->second;

      // if not, just return the non-specialised version
      return m_Reflections[{entry, ResourceId()}];
    }

    rdcspv::Reflector spirv;

    rdcstr unstrippedPath;

    std::map<ShaderModuleReflectionKey, ShaderModuleReflection> m_Reflections;
  };
  std::unordered_map<ResourceId, ShaderModule> m_ShaderModule;

  struct DescSetPool
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkDescriptorPoolCreateInfo *pCreateInfo);

    uint32_t maxSets;
    rdcarray<VkDescriptorPoolSize> poolSizes;

    void CreateOverflow(VkDevice device, VulkanResourceManager *resourceMan);

    rdcarray<VkDescriptorPool> overflow;
  };
  std::unordered_map<ResourceId, DescSetPool> m_DescSetPool;

  struct QueryPool
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkQueryPoolCreateInfo *pCreateInfo);

    VkQueryType queryType;
    uint32_t queryCount;
    VkQueryPipelineStatisticFlags pipelineStatistics;
  };
  std::unordered_map<ResourceId, QueryPool> m_QueryPool;

  std::unordered_map<ResourceId, rdcstr> m_Names;
  std::unordered_map<ResourceId, SwapchainInfo> m_SwapChain;
  std::unordered_map<ResourceId, DescSetLayout> m_DescSetLayout;
  std::unordered_map<ResourceId, DescUpdateTemplate> m_DescUpdateTemplate;

  // just contains the queueFamilyIndex (after remapping)
  std::unordered_map<ResourceId, uint32_t> m_Queue;

  void erase(ResourceId id)
  {
    m_QueryPool.erase(id);
    m_Pipeline.erase(id);
    m_PipelineLayout.erase(id);
    m_RenderPass.erase(id);
    m_Framebuffer.erase(id);
    m_Memory.erase(id);
    m_Buffer.erase(id);
    m_BufferView.erase(id);
    m_Image.erase(id);
    m_Sampler.erase(id);
    m_YCbCrSampler.erase(id);
    m_ImageView.erase(id);
    m_ShaderModule.erase(id);
    m_DescSetPool.erase(id);
    m_Names.erase(id);
    m_SwapChain.erase(id);
    m_DescSetLayout.erase(id);
    m_DescUpdateTemplate.erase(id);
    m_Queue.erase(id);
  }
};
