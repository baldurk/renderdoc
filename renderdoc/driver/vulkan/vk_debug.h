/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

  VkPipeline pipes[ePipe_Count];
};

struct VulkanPostVSData
{
  struct StageData
  {
    VkBuffer buf;
    VkDeviceMemory bufmem;
    VkPrimitiveTopology topo;

    uint32_t numVerts;
    uint32_t vertStride;
    uint32_t instStride;

    bool useIndices;
    VkBuffer idxBuf;
    VkDeviceMemory idxBufMem;
    VkIndexType idxFmt;

    bool hasPosOut;

    float nearPlane;
    float farPlane;
  } vsin, vsout, gsout;

  VulkanPostVSData()
  {
    RDCEraseEl(vsin);
    RDCEraseEl(vsout);
    RDCEraseEl(gsout);
  }

  const StageData &GetStage(MeshDataStage type)
  {
    if(type == MeshDataStage::VSOut)
      return vsout;
    else if(type == MeshDataStage::GSOut)
      return gsout;
    else
      RDCERR("Unexpected mesh data stage!");

    return vsin;
  }
};

struct SPIRVCompilationSettings;

class VulkanResourceManager;

class VulkanDebugManager
{
public:
  VulkanDebugManager(WrappedVulkan *driver, VkDevice dev);
  ~VulkanDebugManager();

  ResourceId RenderOverlay(ResourceId texid, DebugOverlay overlay, uint32_t eventId,
                           const vector<uint32_t> &passEvents);

  void InitPostVSBuffers(uint32_t eventId);

  // indicates that EID alias is the same as eventId
  void AliasPostVSBuffers(uint32_t eventId, uint32_t alias) { m_PostVSAlias[alias] = eventId; }
  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage);
  void ClearPostVSCache();

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret);

  uint32_t PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x, uint32_t y, uint32_t w,
                      uint32_t h);

  void CopyTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent, uint32_t layers,
                          uint32_t samples, VkFormat fmt);
  void CopyArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent, uint32_t layers,
                          uint32_t samples, VkFormat fmt);

  void CreateCustomShaderTex(uint32_t width, uint32_t height, uint32_t mip);
  void CreateCustomShaderPipeline(ResourceId shader);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
  VkSampler m_LinearSampler = VK_NULL_HANDLE, m_PointSampler = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_CheckerboardDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_CheckerboardPipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_CheckerboardDescSet = VK_NULL_HANDLE;
  VkPipeline m_CheckerboardPipeline = VK_NULL_HANDLE;
  VkPipeline m_CheckerboardMSAAPipeline = VK_NULL_HANDLE;
  GPUBuffer m_CheckerboardUBO;

  VkDescriptorSetLayout m_TexDisplayDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_TexDisplayPipeLayout = VK_NULL_HANDLE;
  // ring buffered to allow multiple texture renders between flushes
  VkDescriptorSet m_TexDisplayDescSet[16] = {VK_NULL_HANDLE};
  uint32_t m_TexDisplayNextSet = 0;
  VkPipeline m_TexDisplayPipeline = VK_NULL_HANDLE, m_TexDisplayBlendPipeline = VK_NULL_HANDLE,
             m_TexDisplayF16Pipeline = VK_NULL_HANDLE, m_TexDisplayF32Pipeline = VK_NULL_HANDLE;
  GPUBuffer m_TexDisplayUBO;

  VkImage m_TexDisplayDummyImages[12] = {VK_NULL_HANDLE};
  VkImageView m_TexDisplayDummyImageViews[12] = {VK_NULL_HANDLE};
  VkWriteDescriptorSet m_TexDisplayDummyWrites[12] = {};
  VkDescriptorImageInfo m_TexDisplayDummyInfos[12] = {};
  VkDeviceMemory m_TexDisplayDummyMemory = VK_NULL_HANDLE;

  VkDescriptorSet GetTexDisplayDescSet()
  {
    m_TexDisplayNextSet = (m_TexDisplayNextSet + 1) % ARRAY_COUNT(m_TexDisplayDescSet);
    return m_TexDisplayDescSet[m_TexDisplayNextSet];
  }

  uint32_t m_CustomTexWidth = 0, m_CustomTexHeight = 0;
  VkDeviceSize m_CustomTexMemSize = 0;
  VkImage m_CustomTexImg = VK_NULL_HANDLE;
  VkImageView m_CustomTexImgView[16] = {VK_NULL_HANDLE};
  VkDeviceMemory m_CustomTexMem = VK_NULL_HANDLE;
  VkFramebuffer m_CustomTexFB = VK_NULL_HANDLE;
  VkRenderPass m_CustomTexRP = VK_NULL_HANDLE;
  ResourceId m_CustomTexShader;
  VkPipeline m_CustomTexPipeline = VK_NULL_HANDLE;

  VkDeviceMemory m_PickPixelImageMem = VK_NULL_HANDLE;
  VkImage m_PickPixelImage = VK_NULL_HANDLE;
  VkImageView m_PickPixelImageView = VK_NULL_HANDLE;
  GPUBuffer m_PickPixelReadbackBuffer;
  VkFramebuffer m_PickPixelFB = VK_NULL_HANDLE;
  VkRenderPass m_PickPixelRP = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_ArrayMSDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_ArrayMSPipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_ArrayMSDescSet = VK_NULL_HANDLE;
  VkPipeline m_Array2MSPipe = VK_NULL_HANDLE;
  VkPipeline m_MS2ArrayPipe = VK_NULL_HANDLE;

  // one per depth/stencil output format
  VkPipeline m_DepthMS2ArrayPipe[6] = {VK_NULL_HANDLE};
  // one per depth/stencil output format, per sample count
  VkPipeline m_DepthArray2MSPipe[6][4] = {{VK_NULL_HANDLE}};

  VkDeviceMemory m_OverlayImageMem = VK_NULL_HANDLE;
  VkImage m_OverlayImage = VK_NULL_HANDLE;
  VkImageView m_OverlayImageView = VK_NULL_HANDLE;
  VkFramebuffer m_OverlayNoDepthFB = VK_NULL_HANDLE;
  VkRenderPass m_OverlayNoDepthRP = VK_NULL_HANDLE;
  VkExtent2D m_OverlayDim = {0, 0};
  VkDeviceSize m_OverlayMemSize = 0;

  GPUBuffer m_OverdrawRampUBO;
  VkDescriptorSetLayout m_QuadDescSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_QuadDescSet = VK_NULL_HANDLE;
  VkPipelineLayout m_QuadResolvePipeLayout = VK_NULL_HANDLE;
  VkPipeline m_QuadResolvePipeline[8] = {VK_NULL_HANDLE};

  GPUBuffer m_TriSizeUBO;
  VkDescriptorSetLayout m_TriSizeDescSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_TriSizeDescSet = VK_NULL_HANDLE;
  VkPipelineLayout m_TriSizePipeLayout = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_MeshDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_MeshPipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_MeshDescSet = VK_NULL_HANDLE;
  GPUBuffer m_MeshUBO, m_MeshBBoxVB, m_MeshAxisFrustumVB;

  enum TextureType
  {
    eTexType_1D = 1,    // implicitly an array
    eTexType_2D,        // implicitly an array
    eTexType_3D,
    eTexType_2DMS,
    eTexType_Max
  };

  GPUBuffer m_MinMaxTileResult;                     // tile result buffer
  GPUBuffer m_MinMaxResult, m_MinMaxReadback;       // Vec4f[2] final result buffer
  GPUBuffer m_HistogramBuf, m_HistogramReadback;    // uint32_t * num buckets buffer
  VkDescriptorSetLayout m_HistogramDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_HistogramPipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_HistogramDescSet[2] = {VK_NULL_HANDLE};
  GPUBuffer m_HistogramUBO;
  VkPipeline m_HistogramPipe[eTexType_Max][3] = {{VK_NULL_HANDLE}};     // float, uint, sint
  VkPipeline m_MinMaxTilePipe[eTexType_Max][3] = {{VK_NULL_HANDLE}};    // float, uint, sint
  VkPipeline m_MinMaxResultPipe[3] = {VK_NULL_HANDLE};                  // float, uint, sint

  static const int maxMeshPicks = 500;

  GPUBuffer m_MeshPickUBO;
  GPUBuffer m_MeshPickIB, m_MeshPickIBUpload;
  GPUBuffer m_MeshPickVB, m_MeshPickVBUpload;
  uint32_t m_MeshPickIBSize = 0, m_MeshPickVBSize = 0;
  GPUBuffer m_MeshPickResult, m_MeshPickResultReadback;
  VkDescriptorSetLayout m_MeshPickDescSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_MeshPickDescSet = VK_NULL_HANDLE;
  VkPipelineLayout m_MeshPickLayout = VK_NULL_HANDLE;
  VkPipeline m_MeshPickPipeline = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_OutlineDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_OutlinePipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_OutlineDescSet = VK_NULL_HANDLE;
  VkPipeline m_OutlinePipeline[8] = {VK_NULL_HANDLE};
  GPUBuffer m_OutlineUBO;

  GPUBuffer m_ReadbackWindow;

  VkDescriptorSetLayout m_MeshFetchDescSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_MeshFetchDescSet = VK_NULL_HANDLE;

  MeshDisplayPipelines CacheMeshDisplayPipelines(const MeshFormat &primary,
                                                 const MeshFormat &secondary);

private:
  void InitDebugData();
  void ShutdownDebugData();

  VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
  void CopyDepthTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent, uint32_t layers,
                               uint32_t samples, VkFormat fmt);
  void CopyDepthArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent, uint32_t layers,
                               uint32_t samples, VkFormat fmt);

  void PatchFixedColShader(VkShaderModule &mod, float col[4]);
  void PatchLineStripIndexBuffer(const DrawcallDescription *draw, GPUBuffer &indexBuffer,
                                 uint32_t &indexCount);

  CaptureState m_State;

  std::map<uint64_t, MeshDisplayPipelines> m_CachedMeshPipelines;

  std::map<uint32_t, VulkanPostVSData> m_PostVSData;
  std::map<uint32_t, uint32_t> m_PostVSAlias;

  WrappedVulkan *m_pDriver = NULL;
  VulkanResourceManager *m_ResourceManager = NULL;

  VkDevice m_Device = VK_NULL_HANDLE;
};
