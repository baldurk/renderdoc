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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "vk_common.h"
#include "vk_core.h"

struct MeshDisplayPipelines
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
};

struct SPIRVCompilationSettings;

class VulkanResourceManager;

class VulkanDebugManager
{
public:
  VulkanDebugManager(WrappedVulkan *driver);
  ~VulkanDebugManager();

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret);

  bool IsMS2ArraySupported() { return m_MS2ArrayPipe != VK_NULL_HANDLE; }
  void CopyTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent, uint32_t layers,
                          uint32_t samples, VkFormat fmt);
  void CopyArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent, uint32_t layers,
                          uint32_t samples, VkFormat fmt);

  VkPipeline GetCustomPipeline() { return m_Custom.TexPipeline; }
  VkImage GetCustomTexture() { return m_Custom.TexImg; }
  VkFramebuffer GetCustomFramebuffer() { return m_Custom.TexFB; }
  VkRenderPass GetCustomRenderpass() { return m_Custom.TexRP; }
  void CreateCustomShaderTex(uint32_t width, uint32_t height, uint32_t mip);
  void CreateCustomShaderPipeline(ResourceId shader, VkPipelineLayout pipeLayout);

  MeshDisplayPipelines CacheMeshDisplayPipelines(VkPipelineLayout pipeLayout,
                                                 const MeshFormat &primary,
                                                 const MeshFormat &secondary);

  void PatchFixedColShader(VkShaderModule &mod, float col[4]);
  void PatchLineStripIndexBuffer(const DrawcallDescription *draw, GPUBuffer &indexBuffer,
                                 uint32_t &indexCount);

private:
  // GetBufferData
  GPUBuffer m_ReadbackWindow;

  // CacheMeshDisplayPipelines
  std::map<uint64_t, MeshDisplayPipelines> m_CachedMeshPipelines;

  // CopyArrayToTex2DMS & CopyTex2DMSToArray
  VkDescriptorPool m_ArrayMSDescriptorPool;

  VkDescriptorSetLayout m_ArrayMSDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_ArrayMSPipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_ArrayMSDescSet = VK_NULL_HANDLE;
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
};
