/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
  VkDynamicCount,
};

VkDynamicState ConvertDynamicState(VulkanDynamicStateIndex idx);
VulkanDynamicStateIndex ConvertDynamicState(VkDynamicState state);

struct DescSetLayout
{
  void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
            const VkDescriptorSetLayoutCreateInfo *pCreateInfo);

  void CreateBindingsArray(std::vector<DescriptorSetBindingElement *> &descBindings) const;
  void UpdateBindingsArray(const DescSetLayout &prevLayout,
                           std::vector<DescriptorSetBindingElement *> &descBindings) const;

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
  std::vector<Binding> bindings;

  uint32_t dynamicCount;
  VkDescriptorSetLayoutCreateFlags flags;

  bool operator==(const DescSetLayout &other) const;
  bool operator!=(const DescSetLayout &other) const { return !(*this == other); }
};

struct DescUpdateTemplateApplication
{
  std::vector<VkDescriptorBufferInfo> bufInfo;
  std::vector<VkDescriptorImageInfo> imgInfo;
  std::vector<VkBufferView> bufView;

  std::vector<VkWriteDescriptorSet> writes;
};

struct DescUpdateTemplate
{
  void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
            const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo);

  void Apply(const void *pData, DescUpdateTemplateApplication &application);

  DescSetLayout layout;

  VkPipelineBindPoint bindPoint;

  size_t dataByteSize;

  uint32_t texelBufferViewCount;
  uint32_t bufferInfoCount;
  uint32_t imageInfoCount;

  std::vector<VkDescriptorUpdateTemplateEntry> updates;
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
      std::string entryPoint;
      ShaderReflection *refl;
      ShaderBindpointMapping *mapping;
      SPIRVPatchData *patchData;

      std::vector<SpecConstant> specialization;
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
    std::vector<Binding> vertexBindings;

    struct Attribute
    {
      uint32_t location;
      uint32_t binding;
      VkFormat format;
      uint32_t byteoffset;
    };
    std::vector<Attribute> vertexAttrs;

    // VkPipelineInputAssemblyStateCreateInfo
    VkPrimitiveTopology topology;
    bool primitiveRestartEnable;

    // VkPipelineTessellationStateCreateInfo
    uint32_t patchControlPoints;

    // VkPipelineTessellationDomainOriginStateCreateInfo
    VkTessellationDomainOrigin tessellationDomainOrigin;

    // VkPipelineViewportStateCreateInfo
    uint32_t viewportCount;
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;

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
      std::vector<VkSampleLocationEXT> locations;
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
    std::vector<Attachment> attachments;

    // VkPipelineDynamicStateCreateInfo
    bool dynamicStates[VkDynamicCount];

    // VkPipelineDiscardRectangleStateCreateInfoEXT
    std::vector<VkRect2D> discardRectangles;
    VkDiscardRectangleModeEXT discardMode;
  };
  std::map<ResourceId, Pipeline> m_Pipeline;

  struct PipelineLayout
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkPipelineLayoutCreateInfo *pCreateInfo);

    std::vector<VkPushConstantRange> pushRanges;
    std::vector<ResourceId> descSetLayouts;
  };
  std::map<ResourceId, PipelineLayout> m_PipelineLayout;

  struct RenderPass
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkRenderPassCreateInfo *pCreateInfo);
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkRenderPassCreateInfo2KHR *pCreateInfo);

    struct Attachment
    {
      VkAttachmentDescriptionFlags flags;
      VkFormat format;
      VkSampleCountFlagBits samples;
      VkAttachmentLoadOp loadOp;
      VkAttachmentStoreOp storeOp;
      VkAttachmentLoadOp stencilLoadOp;
      VkAttachmentStoreOp stencilStoreOp;
      VkImageLayout initialLayout;
      VkImageLayout finalLayout;
    };

    std::vector<Attachment> attachments;

    struct Subpass
    {
      // these are split apart since they layout is
      // rarely used but the indices are often used
      std::vector<uint32_t> inputAttachments;
      std::vector<uint32_t> colorAttachments;
      std::vector<uint32_t> resolveAttachments;
      int32_t depthstencilAttachment;
      int32_t fragmentDensityAttachment;

      std::vector<VkImageLayout> inputLayouts;
      std::vector<VkImageLayout> colorLayouts;
      VkImageLayout depthstencilLayout;
      VkImageLayout fragmentDensityLayout;

      std::vector<uint32_t> multiviews;
    };
    std::vector<Subpass> subpasses;

    // one for each subpass, as we preserve attachments
    // in the layout that the subpass uses
    std::vector<VkRenderPass> loadRPs;
  };
  std::map<ResourceId, RenderPass> m_RenderPass;

  struct Framebuffer
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkFramebufferCreateInfo *pCreateInfo);

    struct Attachment
    {
      ResourceId view;
      VkFormat format;
    };
    std::vector<Attachment> attachments;

    uint32_t width, height, layers;

    // See above in loadRPs - we need to duplicate and make framebuffer equivalents for each
    std::vector<VkFramebuffer> loadFBs;
  };
  std::map<ResourceId, Framebuffer> m_Framebuffer;

  struct Memory
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkMemoryAllocateInfo *pAllocInfo);

    uint32_t memoryTypeIndex;
    uint64_t size;

    VkBuffer wholeMemBuf;
  };
  std::map<ResourceId, Memory> m_Memory;

  struct Buffer
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkBufferCreateInfo *pCreateInfo);

    VkBufferUsageFlags usage;
    uint64_t size;
  };
  std::map<ResourceId, Buffer> m_Buffer;

  struct BufferView
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkBufferViewCreateInfo *pCreateInfo);

    ResourceId buffer;
    VkFormat format;
    uint64_t offset;
    uint64_t size;
  };
  std::map<ResourceId, BufferView> m_BufferView;

  struct Image
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkImageCreateInfo *pCreateInfo);

    VkImageType type;
    VkFormat format;
    VkExtent3D extent;
    int arrayLayers, mipLevels;
    VkSampleCountFlagBits samples;

    bool cube;
    TextureCategory creationFlags;
  };
  std::map<ResourceId, Image> m_Image;

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
    VkSamplerReductionModeEXT reductionMode;

    ResourceId ycbcr;
  };
  std::map<ResourceId, Sampler> m_Sampler;

  struct YCbCrSampler
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkSamplerYcbcrConversionCreateInfo *pCreateInfo);

    YcbcrConversion ycbcrModel;
    YcbcrRange ycbcrRange;
    TextureSwizzle swizzle[4];
    ChromaSampleLocation xChromaOffset;
    ChromaSampleLocation yChromaOffset;
    FilterMode chromaFilter;
    bool forceExplicitReconstruction;
  };
  std::map<ResourceId, YCbCrSampler> m_YCbCrSampler;

  struct ImageView
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkImageViewCreateInfo *pCreateInfo);

    ResourceId image;
    VkFormat format;
    VkImageSubresourceRange range;
    TextureSwizzle swizzle[4];
  };
  std::map<ResourceId, ImageView> m_ImageView;

  struct ShaderModule
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkShaderModuleCreateInfo *pCreateInfo);

    SPVModule spirv;

    std::string unstrippedPath;

    struct Reflection
    {
      uint32_t stageIndex;
      std::string entryPoint;
      std::string disassembly;
      ShaderReflection refl;
      ShaderBindpointMapping mapping;
      SPIRVPatchData patchData;

      void Init(VulkanResourceManager *resourceMan, ResourceId id, const SPVModule &spv,
                const std::string &entry, VkShaderStageFlagBits stage);
    };
    std::map<std::string, Reflection> m_Reflections;
  };
  std::map<ResourceId, ShaderModule> m_ShaderModule;

  struct DescSetPool
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkDescriptorPoolCreateInfo *pCreateInfo);

    uint32_t maxSets;
    std::vector<VkDescriptorPoolSize> poolSizes;

    void CreateOverflow(VkDevice device, VulkanResourceManager *resourceMan);

    std::vector<VkDescriptorPool> overflow;
  };
  std::map<ResourceId, DescSetPool> m_DescSetPool;

  std::map<ResourceId, std::string> m_Names;
  std::map<ResourceId, SwapchainInfo> m_SwapChain;
  std::map<ResourceId, DescSetLayout> m_DescSetLayout;
  std::map<ResourceId, DescUpdateTemplate> m_DescUpdateTemplate;

  // just contains the queueFamilyIndex (after remapping)
  std::map<ResourceId, uint32_t> m_Queue;

  void erase(ResourceId id)
  {
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
