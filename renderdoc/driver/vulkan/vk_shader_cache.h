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

#include "core/core.h"
#include "driver/shaders/spirv/spirv_compile.h"
#include "vk_core.h"

typedef rdcarray<uint32_t> *SPIRVBlob;

enum class BuiltinShader
{
  BlitVS,
  First = BlitVS,
  CheckerboardFS,
  TexDisplayFS,
  FixedColFS,
  TextVS,
  TextFS,
  MeshVS,
  MeshGS,
  MeshFS,
  MeshCS,
  QuadResolveFS,
  QuadWriteFS,
  TrisizeGS,
  TrisizeFS,
  TexRemap,
  PixelHistoryMSCopyCS,
  PixelHistoryMSCopyDepthCS,
  PixelHistoryPrimIDFS,
  ShaderDebugSampleVS,
  DiscardFS,
  HistogramCS,
  MinMaxTileCS,
  MinMaxResultCS,
  MS2BufferCS,
  DepthMS2BufferCS,
  Buffer2MSCS,
  DepthBuf2MSFS,
  DepthCopyFS,
  DepthCopyMSFS,
  Count,
};

ITERABLE_OPERATORS(BuiltinShader);

enum class BuiltinShaderBaseType
{
  Float = 0,
  First = Float,
  UInt,
  SInt,
  Count,
};

ITERABLE_OPERATORS(BuiltinShaderBaseType);

enum class BuiltinShaderTextureType
{
  Tex1D = 1,
  First = Tex1D,
  Tex2D,
  Tex3D,
  Tex2DMS,
  Count,
};

ITERABLE_OPERATORS(BuiltinShaderTextureType);

class VulkanShaderCache
{
public:
  VulkanShaderCache(WrappedVulkan *driver);
  ~VulkanShaderCache();

  rdcstr GetSPIRVBlob(const rdcspv::CompilationSettings &settings, const rdcstr &src,
                      SPIRVBlob &outBlob);

  SPIRVBlob GetBuiltinBlob(BuiltinShader builtin)
  {
    return m_BuiltinShaderBlobs[(size_t)builtin][(size_t)BuiltinShaderBaseType::First]
                               [(size_t)BuiltinShaderTextureType::First];
  }
  VkShaderModule GetBuiltinModule(BuiltinShader builtin,
                                  BuiltinShaderBaseType baseType = BuiltinShaderBaseType::First,
                                  BuiltinShaderTextureType texType = BuiltinShaderTextureType::First)
  {
    return m_BuiltinShaderModules[(size_t)builtin][(size_t)baseType][(size_t)texType];
  }
  VkPipelineCache GetPipeCache() { return m_PipelineCache; }
  void MakeGraphicsPipelineInfo(VkGraphicsPipelineCreateInfo &pipeCreateInfo, ResourceId pipeline);
  void MakeComputePipelineInfo(VkComputePipelineCreateInfo &pipeCreateInfo, ResourceId pipeline);
  void MakeShaderObjectInfo(VkShaderCreateInfoEXT &shadCreateInfo, ResourceId shader);

  bool IsBuffer2MSSupported() { return m_Buffer2MSSupported; }
  void SetCaching(bool enabled) { m_CacheShaders = enabled; }
private:
  static const uint32_t m_ShaderCacheMagic = 0xf00d00d5;
  static const uint32_t m_ShaderCacheVersion = 1;

  void GetPipeCacheBlob();
  void SetPipeCacheBlob(bytebuf &blob);

  WrappedVulkan *m_pDriver = NULL;
  VkDevice m_Device = VK_NULL_HANDLE;

  // combined pipeline layouts constructed out of independent set pipeline layouts, for use in a
  // single combined graphics pipeline create info
  std::map<ResourceId, VkPipelineLayout> m_CombinedPipeLayouts;

  bytebuf m_PipeCacheBlob;
  VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

  bool m_Buffer2MSSupported = false;

  bool m_ShaderCacheDirty = false, m_CacheShaders = false;
  std::map<uint32_t, SPIRVBlob> m_ShaderCache;

  SPIRVBlob m_BuiltinShaderBlobs[arraydim<BuiltinShader>()][arraydim<BuiltinShaderBaseType>()]
                                [arraydim<BuiltinShaderTextureType>()] = {};
  VkShaderModule m_BuiltinShaderModules[arraydim<BuiltinShader>()][arraydim<BuiltinShaderBaseType>()]
                                       [arraydim<BuiltinShaderTextureType>()] = {};
};
