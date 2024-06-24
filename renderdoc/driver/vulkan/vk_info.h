/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
        : layoutDescType(VK_DESCRIPTOR_TYPE_MAX_ENUM),
          elemOffset(0),
          descriptorCount(0),
          stageFlags(0),
          variableSize(0),
          immutableSampler(NULL)
    {
    }
    // move the immutable sampler
    Binding(Binding &&b)
        : layoutDescType(b.layoutDescType),
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
        : layoutDescType(b.layoutDescType),
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

      layoutDescType = b.layoutDescType;
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
    // this is the layout-declared type, but since it may be mutable in most cases this is not used
    // - only push descriptors use this
    VkDescriptorType layoutDescType;
    uint32_t elemOffset;
    uint32_t descriptorCount;
    VkShaderStageFlags stageFlags : 31;
    uint32_t variableSize : 1;
    ResourceId *immutableSampler;

    inline uint32_t GetDescriptorCount(uint32_t varDescriptorSize) const
    {
      if(variableSize)
        return varDescriptorSize;
      return descriptorCount;
    }
  };
  rdcarray<Binding> bindings;

  // parallel array to bindings, with a bitmask of mutable types
  rdcarray<uint64_t> mutableBitmasks;

  uint32_t totalElems;
  uint32_t dynamicCount;
  VkDescriptorSetLayoutCreateFlags flags;

  uint32_t inlineCount;
  uint32_t inlineByteSize;

  uint32_t accelerationStructureWriteCount;
  uint32_t accelerationStructureCount;

  // the cummulative stageFlags for all bindings in this layout
  VkShaderStageFlags anyStageFlags;

  bool isCompatible(const DescSetLayout &other) const;
};

bool IsValid(bool allowNULLDescriptors, const VkWriteDescriptorSet &write, uint32_t arrayElement);
bool CreateDescriptorWritesForSlotData(WrappedVulkan *vk, rdcarray<VkWriteDescriptorSet> &writes,
                                       VkDescriptorBufferInfo *&writeScratch,
                                       const DescriptorSetSlot *slots, uint32_t descriptorCount,
                                       VkDescriptorSet set, uint32_t dstBind,
                                       const DescSetLayout::Binding &layoutBind);

struct DescUpdateTemplateApplication
{
  rdcarray<VkDescriptorBufferInfo> bufInfo;
  rdcarray<VkDescriptorImageInfo> imgInfo;
  rdcarray<VkBufferView> bufView;
  rdcarray<VkWriteDescriptorSetInlineUniformBlock> inlineUniform;
  bytebuf inlineData;

  rdcarray<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructureWrite;
  rdcarray<VkAccelerationStructureKHR> accelerationStructure;

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
  uint32_t accelerationStructureWriteCount;
  uint32_t accelerationStructureCount;

  rdcarray<VkDescriptorUpdateTemplateEntry> updates;
};

struct VulkanCreationInfo
{
  struct ShaderModuleReflectionKey
  {
    ShaderModuleReflectionKey(ShaderStage s, const rdcstr &e, ResourceId p)
        : stage(s), entryPoint(e), specialisingPipe(p)
    {
    }
    bool operator<(const ShaderModuleReflectionKey &o) const
    {
      if(entryPoint != o.entryPoint)
        return entryPoint < o.entryPoint;
      if(stage != o.stage)
        return stage < o.stage;

      return specialisingPipe < o.specialisingPipe;
    }

    // stage of the entry point
    ShaderStage stage;
    // name of the entry point
    rdcstr entryPoint;
    // ID of the pipeline ONLY if it contains specialisation constant data
    ResourceId specialisingPipe;
  };

  struct ShaderModuleReflection
  {
    ShaderModuleReflection() { refl = new ShaderReflection; }
    ~ShaderModuleReflection() { SAFE_DELETE(refl); }
    ShaderModuleReflection(const ShaderModuleReflection &o) = delete;
    ShaderModuleReflection &operator=(const ShaderModuleReflection &o) = delete;

    uint32_t stageIndex;
    rdcstr entryPoint;
    rdcstr disassembly;
    ShaderReflection *refl;
    SPIRVPatchData patchData;
    std::map<size_t, uint32_t> instructionLines;

    void Init(VulkanResourceManager *resourceMan, ResourceId id, const rdcspv::Reflector &spv,
              const rdcstr &entry, VkShaderStageFlagBits stage,
              const rdcarray<SpecConstant> &specInfo);

    void PopulateDisassembly(const rdcspv::Reflector &spirv);
  };

  struct ShaderEntry
  {
    ResourceId module;
    ShaderStage stage = ShaderStage::Count;
    rdcstr entryPoint;
    ShaderReflection *refl = NULL;
    SPIRVPatchData *patchData = NULL;

    rdcarray<SpecConstant> specialization;

    // VkPipelineShaderStageRequiredSubgroupSizeCreateInfo
    uint32_t requiredSubgroupSize = 0;

    void ProcessStaticDescriptorAccess(ResourceId pushStorage, ResourceId specStorage,
                                       rdcarray<DescriptorAccess> &staticDescriptorAccess,
                                       rdcarray<const DescSetLayout *> setLayoutInfos) const;
  };

  struct Pipeline
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, ResourceId id,
              const VkGraphicsPipelineCreateInfo *pCreateInfo);
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, ResourceId id,
              const VkComputePipelineCreateInfo *pCreateInfo);
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, ResourceId id,
              const VkRayTracingPipelineCreateInfoKHR *pCreateInfo);

    bool graphicsPipe = false;

    //  VkGraphicsPipelineLibraryCreateInfoEXT
    VkGraphicsPipelineLibraryFlagsEXT availStages;
    rdcarray<ResourceId> parentLibraries;

    ResourceId compLayout;

    // these will be the same in some cases, but can be different if the application is using
    // INDEPENDENT_SETS_BIT_KHR
    ResourceId vertLayout;
    ResourceId fragLayout;

    // this is the list of descriptor set layouts for a 'complete' pipeline.
    // when vertLayout == fragLayout (i.e. no independent sets), for compute pipelines, or if only
    // one is set, then this will be trivially equal to the set of layouts in the pipeline layout.
    // when they are different and both non-empty, it will be the list of descriptor sets
    // cherry-picked from each. Specifically all of the descriptor sets that have fragment shader
    // bindings taken from the fragment layout, and then all of the remaining sets from the vertex
    // layout
    rdcarray<ResourceId> descSetLayouts;

    ResourceId renderpass;
    uint32_t subpass;

    // VkPipelineRenderingCreateInfoKHR
    uint32_t viewMask;
    rdcarray<VkFormat> colorFormats;
    VkFormat depthFormat;
    VkFormat stencilFormat;

    // a variant of the pipeline that uses subpass 0, used for when we are replaying in isolation.
    // See loadRPs in the RenderPass info
    VkPipeline subpass0pipe;

    // VkGraphicsPipelineCreateInfo
    VkPipelineCreateFlags flags;

    // VkPipelineShaderStageCreateInfo
    ShaderEntry shaders[NumShaderStages];

    // this is the total size of the 'virtualised' specialisation data, where all constants are stored
    // 64-bit aligned and with an offset equal to their ID. In other words this is big enough for the max ID
    uint32_t virtualSpecialisationByteSize = 0;

    rdcarray<DescriptorAccess> staticDescriptorAccess;

    // VkPipelineVertexInputStateCreateInfo
    struct VertBinding
    {
      uint32_t vbufferBinding;
      uint32_t bytestride;
      bool perInstance;

      // VkVertexInputBindingDivisorDescriptionEXT
      uint32_t instanceDivisor;
    };
    rdcarray<VertBinding> vertexBindings;

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

    // VkPipelineRasterizationLineStateCreateInfoKHR
    VkLineRasterizationModeKHR lineRasterMode;
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

    struct CBAttachment
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
    rdcarray<CBAttachment> attachments;

    // VkPipelineDynamicStateCreateInfo
    bool dynamicStates[VkDynamicCount];

    // VkPipelineDiscardRectangleStateCreateInfoEXT
    rdcarray<VkRect2D> discardRectangles;
    VkDiscardRectangleModeEXT discardMode;

    // VkPipelineFragmentShadingRateCreateInfoKHR
    VkExtent2D shadingRate;
    VkFragmentShadingRateCombinerOpKHR shadingRateCombiners[2];

    // VkPipelineViewportDepthClipControlCreateInfoEXT
    bool negativeOneToOne;

    // VkPipelineRasterizationProvokingVertexStateCreateInfoEXT
    VkProvokingVertexModeEXT provokingVertex;
  };
  std::unordered_map<ResourceId, Pipeline> m_Pipeline;

  struct ShaderObject
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, ResourceId id,
              const VkShaderCreateInfoEXT *pCreateInfo);

    ShaderEntry shad;

    VkShaderCreateFlagsEXT flags;

    VkShaderStageFlags nextStage;

    VkShaderCodeTypeEXT codeType = VK_SHADER_CODE_TYPE_MAX_ENUM_EXT;

    rdcarray<VkPushConstantRange> pushRanges;
    rdcarray<ResourceId> descSetLayouts;

    // this is the total size of the 'virtualised' specialisation data, where all constants are stored
    // 64-bit aligned and with an offset equal to their ID. In other words this is big enough for the max ID
    uint32_t virtualSpecialisationByteSize = 0;

    rdcarray<DescriptorAccess> staticDescriptorAccess;
  };
  std::unordered_map<ResourceId, ShaderObject> m_ShaderObject;

  struct PipelineLayout
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkPipelineLayoutCreateInfo *pCreateInfo);

    VkPipelineLayoutCreateFlags flags;
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
      int32_t depthstencilResolveAttachment;
      int32_t fragmentDensityAttachment;
      int32_t shadingRateAttachment;
      VkSampleCountFlagBits tileOnlyMSAASampleCount;

      rdcarray<VkImageLayout> inputLayouts;
      rdcarray<VkImageLayout> inputStencilLayouts;
      rdcarray<VkImageLayout> colorLayouts;
      VkImageLayout depthLayout;
      VkImageLayout stencilLayout;
      VkImageLayout fragmentDensityLayout;
      VkImageLayout shadingRateLayout;

      VkExtent2D shadingRateTexelSize;

      rdcarray<uint32_t> multiviews;

      bool feedbackLoop;
      bool tileOnlyMSAAEnable;
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

    // allocSize is the raw size of this allocation, useful for checking if resources fit.
    uint64_t allocSize;
    // wholeMemBufSize is the size of wholeMemBuf, which could be lower in the case of dedicated
    // buffers with larger memory allocations than the buffer
    uint64_t wholeMemBufSize;

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
              const VkBufferCreateInfo *pCreateInfo, VkMemoryRequirements origMrq);

    VkBufferUsageFlags usage;
    uint64_t size;
    uint64_t gpuAddress;
    bool external;

    VkMemoryRequirements mrq;
  };
  std::unordered_map<ResourceId, Buffer> m_Buffer;
  rdcsortedflatmap<uint64_t, ResourceId> m_BufferAddresses;

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
              const VkImageCreateInfo *pCreateInfo, VkMemoryRequirements origMrq);

    VkImageType type;
    VkFormat format;
    VkExtent3D extent;
    uint32_t arrayLayers, mipLevels;
    VkSampleCountFlagBits samples;

    bool linear;
    bool external;
    bool cube;
    TextureCategory creationFlags;

    VkMemoryRequirements mrq;
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

    // VkSamplerBorderColorComponentMappingCreateInfoEXT
    VkComponentMapping componentMapping;
    bool srgbBorder;

    // VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT
    bool seamless;
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

    // VkImageViewMinLodCreateInfoEXT
    float minLOD;
  };
  std::unordered_map<ResourceId, ImageView> m_ImageView;

  struct ShaderModule
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkShaderModuleCreateInfo *pCreateInfo);

    void Reinit();

    ShaderModuleReflection &GetReflection(ShaderStage stage, const rdcstr &entry, ResourceId pipe)
    {
      auto redirIt = m_PipeReferences.find(pipe);
      if(redirIt != m_PipeReferences.end())
        pipe = redirIt->second;

      // look for one from this pipeline specifically, if it was specialised
      auto it = m_Reflections.find({stage, entry, pipe});
      if(it != m_Reflections.end())
        return it->second;

      // if not, just return the non-specialised version
      return m_Reflections[{stage, entry, ResourceId()}];
    }

    rdcspv::Reflector spirv;

    rdcstr unstrippedPath;

    std::map<ShaderModuleReflectionKey, ShaderModuleReflection> m_Reflections;
    // in graphics pipeline library the linked pipeline may reference a different pipeline where the
    // shaders are. So when looking up the reflection as specialised by a given pipeline we may want
    // to redirect to the 'real' pipeline that specialised it.
    std::unordered_map<ResourceId, ResourceId> m_PipeReferences;
  };
  std::unordered_map<ResourceId, ShaderModule> m_ShaderModule;

  struct DescSetPool
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkDescriptorPoolCreateInfo *pCreateInfo);

    uint32_t maxSets;
    rdcarray<VkDescriptorPoolSize> poolSizes;
    rdcarray<uint64_t> mutableBitmasks;

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

  struct AccelerationStructure
  {
    void Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
              const VkAccelerationStructureCreateInfoKHR *pCreateInfo);

    ResourceId buffer;
    uint64_t offset;
    uint64_t size;
    VkAccelerationStructureTypeKHR type;
  };
  std::unordered_map<ResourceId, AccelerationStructure> m_AccelerationStructure;

  std::unordered_map<ResourceId, rdcstr> m_Names;
  std::unordered_map<ResourceId, SwapchainInfo> m_SwapChain;
  std::unordered_map<ResourceId, DescSetLayout> m_DescSetLayout;
  std::unordered_map<ResourceId, DescUpdateTemplate> m_DescUpdateTemplate;

  // just contains the queueFamilyIndex (after remapping)
  std::unordered_map<ResourceId, uint32_t> m_Queue;

  // the fake ID of the 'command buffer' descriptor store for push constants
  ResourceId pushConstantDescriptorStorage;

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
    m_ShaderObject.erase(id);
    m_DescSetPool.erase(id);
    m_AccelerationStructure.erase(id);
    m_Names.erase(id);
    m_SwapChain.erase(id);
    m_DescSetLayout.erase(id);
    m_DescUpdateTemplate.erase(id);
    m_Queue.erase(id);
  }
};
