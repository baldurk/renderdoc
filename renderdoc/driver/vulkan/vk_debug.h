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

  void CopyTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent, uint32_t layers,
                          uint32_t samples, VkFormat fmt);
  void CopyArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent, uint32_t layers,
                          uint32_t samples, VkFormat fmt);

  void FillWithDiscardPattern(VkCommandBuffer cmd, DiscardType type, VkImage image,
                              VkImageLayout curLayout, VkImageSubresourceRange discardRange,
                              VkRect2D discardRect);

  void InitReadbackBuffer();
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

  void PatchOutputLocation(VkShaderModule &mod, BuiltinShader shaderType, uint32_t framebufferIndex);
  void PatchFixedColShader(VkShaderModule &mod, float col[4]);
  void PatchLineStripIndexBuffer(const DrawcallDescription *draw, GPUBuffer &indexBuffer,
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
  // GetBufferData
  GPUBuffer m_ReadbackWindow;
  byte *m_ReadbackPtr = NULL;

  // CacheMeshDisplayPipelines
  std::map<uint64_t, VKMeshDisplayPipelines> m_CachedMeshPipelines;

  // CopyArrayToTex2DMS & CopyTex2DMSToArray
  VkDescriptorPool m_ArrayMSDescriptorPool;
  VkDescriptorSetLayout m_ArrayMSDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_ArrayMSPipeLayout = VK_NULL_HANDLE;
  // 8 descriptor sets allows for 4x MSAA with 2 array slices, common for VR targets
  VkDescriptorSet m_ArrayMSDescSet[8] = {};
  VkPipeline m_Array2MSPipe = VK_NULL_HANDLE;
  VkPipeline m_MS2ArrayPipe = VK_NULL_HANDLE;
  VkSampler m_ArrayMSSampler = VK_NULL_HANDLE;

  // [0] = non-MSAA, [1] = MSAA
  VkDeviceMemory m_DummyStencilMemory = VK_NULL_HANDLE;
  VkImage m_DummyStencilImage[2] = {VK_NULL_HANDLE};
  VkImageView m_DummyStencilView[2] = {VK_NULL_HANDLE};

  // one per depth/stencil output format
  VkPipeline m_DepthMS2ArrayPipe[6] = {VK_NULL_HANDLE};
  // one per depth/stencil output format, per sample count
  VkPipeline m_DepthArray2MSPipe[6][4] = {{VK_NULL_HANDLE}};

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

  void CopyDepthTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent, uint32_t layers,
                               uint32_t samples, VkFormat fmt);
  void CopyDepthArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent, uint32_t layers,
                               uint32_t samples, VkFormat fmt);

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
};
