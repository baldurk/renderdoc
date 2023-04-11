/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "core/core.h"
#include "replay/replay_driver.h"
#include "vk_common.h"
#include "vk_core.h"
#include "vk_shader_cache.h"

struct VKMeshDisplayPipelines
{
  enum
  {
    ePipe_Wire = 0,
    ePipe_WireDepth,
    ePipe_Solid,
    ePipe_SolidDepth,
    ePipe_Lit,
    ePipe_Secondary,
    ePipe_Count,
  };

  VkPipeline pipes[ePipe_Count] = {};

  uint32_t primaryStridePadding = 0;
  uint32_t secondaryStridePadding = 0;
};

struct CopyPixelParams;

struct PixelHistoryResources;

class VulkanResourceManager;

class VulkanDebugManager
{
public:
  VulkanDebugManager(WrappedVulkan *driver);
  ~VulkanDebugManager();

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret);

  void CopyTex2DMSToBuffer(VkCommandBuffer cmd, VkBuffer destBuffer, VkImage srcMS,
                           VkExtent3D extent, uint32_t baseSlice, uint32_t numSlices,
                           uint32_t baseSample, uint32_t numSamples, VkFormat fmt);

  void CopyBufferToTex2DMS(VkCommandBuffer cmd, VkImage destMS, VkBuffer srcBuffer,
                           VkExtent3D extent, uint32_t numSlices, uint32_t numSamples, VkFormat fmt);

  void FillWithDiscardPattern(VkCommandBuffer cmd, DiscardType type, VkImage image,
                              VkImageLayout curLayout, VkImageSubresourceRange discardRange,
                              VkRect2D discardRect);

  void InitReadbackBuffer(VkDeviceSize sz);
  byte *GetReadbackPtr() { return m_ReadbackPtr; }
  VkBuffer GetReadbackBuffer() { return m_ReadbackWindow.buf; }
  VkDeviceMemory GetReadbackMemory() { return m_ReadbackWindow.mem; }
  VkPipelineCache GetPipelineCache() { return m_PipelineCache; }
  VkPipeline GetCustomPipeline() { return m_Custom.TexPipeline; }
  VkImage GetCustomTexture() { return m_Custom.TexImg; }
  VkFramebuffer GetCustomFramebuffer() { return m_Custom.TexFB; }
  VkRenderPass GetCustomRenderpass() { return m_Custom.TexRP; }
  void CreateCustomShaderTex(uint32_t width, uint32_t height, uint32_t mip);
  void CreateCustomShaderPipeline(ResourceId shader, VkPipelineLayout pipeLayout);

  VKMeshDisplayPipelines CacheMeshDisplayPipelines(VkPipelineLayout pipeLayout,
                                                   const MeshFormat &primary,
                                                   const MeshFormat &secondary);

  void PatchFixedColShader(VkShaderModule &mod, float col[4]);
  void PatchLineStripIndexBuffer(const ActionDescription *action, GPUBuffer &indexBuffer,
                                 uint32_t &indexCount);

  bool PixelHistorySetupResources(PixelHistoryResources &resources, VkImage targetImage,
                                  VkExtent3D extent, VkFormat format, VkSampleCountFlagBits samples,
                                  const Subresource &sub, uint32_t numEvents);
  bool PixelHistoryDestroyResources(const PixelHistoryResources &resources);

  void PixelHistoryCopyPixel(VkCommandBuffer cmd, CopyPixelParams &p, size_t offset);

  VkImageLayout GetImageLayout(ResourceId image, VkImageAspectFlagBits aspect, uint32_t mip,
                               uint32_t slice);

  const VulkanCreationInfo::Image &GetImageInfo(ResourceId img) const;
  const VulkanCreationInfo::ImageView &GetImageViewInfo(ResourceId imgView) const;
  const VulkanCreationInfo::Pipeline &GetPipelineInfo(ResourceId pipe) const;
  const VulkanCreationInfo::ShaderModule &GetShaderInfo(ResourceId shader) const;
  const VulkanCreationInfo::Framebuffer &GetFramebufferInfo(ResourceId fb) const;
  const VulkanCreationInfo::RenderPass &GetRenderPassInfo(ResourceId rp) const;
  const VulkanCreationInfo::PipelineLayout &GetPipelineLayoutInfo(ResourceId pp) const;
  const DescSetLayout &GetDescSetLayout(ResourceId dsl) const;
  const WrappedVulkan::DescriptorSetInfo &GetDescSetInfo(ResourceId ds) const;

private:
  void CheckVkResult(VkResult vkr) { return m_pDriver->CheckVkResult(vkr); }
  // GetBufferData
  GPUBuffer m_ReadbackWindow;
  byte *m_ReadbackPtr = NULL;

  // CacheMeshDisplayPipelines
  std::map<uint64_t, VKMeshDisplayPipelines> m_CachedMeshPipelines;

  // CopyBufferToTex2DMS
  VkDescriptorSetLayout m_BufferMSDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_BufferMSPipeLayout = VK_NULL_HANDLE;
  static const uint32_t BufferMSDescriptorPoolSize = 64;
  rdcarray<VkDescriptorPool> m_BufferMSDescriptorPools;
  rdcarray<VkDescriptorSet> m_FreeBufferMSDescriptorSets;
  rdcarray<VkDescriptorSet> m_UsedBufferMSDescriptorSets;
  VkDescriptorSet GetBufferMSDescSet();
  void ResetBufferMSDescriptorPools();
  VkPipeline m_Buffer2MSPipe = VK_NULL_HANDLE;
  VkPipeline m_MS2BufferPipe = VK_NULL_HANDLE;
  VkPipeline m_DepthMS2BufferPipe = VK_NULL_HANDLE;

  // MSAA dummy images
  VkDeviceMemory m_DummyMemory = VK_NULL_HANDLE;
  VkImage m_DummyDepthImage = {VK_NULL_HANDLE};
  VkImageView m_DummyDepthView = {VK_NULL_HANDLE};
  VkImage m_DummyStencilImage = {VK_NULL_HANDLE};
  VkImageView m_DummyStencilView = {VK_NULL_HANDLE};

  // one per depth/stencil output format, per sample count
  VkPipeline m_DepthArray2MSPipe[7][4] = {{VK_NULL_HANDLE}};

  VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

  struct CustomShaderRendering
  {
    void Destroy(WrappedVulkan *driver);

    uint32_t TexWidth = 0, TexHeight = 0;
    VkDeviceSize TexMemSize = 0;
    VkImage TexImg = VK_NULL_HANDLE;
    VkImageView TexImgView[16] = {VK_NULL_HANDLE};
    VkDeviceMemory TexMem = VK_NULL_HANDLE;
    VkFramebuffer TexFB = VK_NULL_HANDLE;
    VkRenderPass TexRP = VK_NULL_HANDLE;
    ResourceId TexShader;
    VkPipeline TexPipeline = VK_NULL_HANDLE;
  } m_Custom;

  void CopyDepthTex2DMSToBuffer(VkCommandBuffer cmd, VkBuffer destBuffer, VkImage srcMS,
                                VkExtent3D extent, uint32_t baseSlice, uint32_t numSlices,
                                uint32_t baseSample, uint32_t numSamples, VkFormat fmt);

  void CopyDepthBufferToTex2DMS(VkCommandBuffer cmd, VkImage destMS, VkBuffer srcBuffer,
                                VkExtent3D extent, uint32_t numSlices, uint32_t numSamples,
                                VkFormat fmt);

  WrappedVulkan *m_pDriver = NULL;

  VkDevice m_Device = VK_NULL_HANDLE;

  struct DiscardPassData
  {
    VkPipeline pso = VK_NULL_HANDLE;
    VkRenderPass rp = VK_NULL_HANDLE;
  };

  struct DiscardImgData
  {
    rdcarray<VkImageView> views;
    rdcarray<VkFramebuffer> fbs;
  };

  std::map<rdcpair<VkFormat, VkSampleCountFlagBits>, DiscardPassData> m_DiscardPipes;
  std::map<ResourceId, DiscardImgData> m_DiscardImages;
  VkDescriptorPool m_DiscardPool = VK_NULL_HANDLE;
  VkPipelineLayout m_DiscardLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_DiscardSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_DiscardSet[(size_t)DiscardType::Count] = {};
  GPUBuffer m_DiscardCB[(size_t)DiscardType::Count];
  std::map<rdcpair<VkFormat, DiscardType>, VkBuffer> m_DiscardPatterns;
  std::map<rdcpair<VkFormat, DiscardType>, GPUBuffer> m_DiscardStage;
};
