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
  MS2ArrayCS,
  Array2MSCS,
  DepthMS2ArrayFS,
  DepthArray2MSFS,
  TexRemapFloat,
  TexRemapUInt,
  TexRemapSInt,
  PixelHistoryMSCopyCS,
  PixelHistoryMSCopyDepthCS,
  PixelHistoryPrimIDFS,
  ShaderDebugSampleVS,
  Count,
};

ITERABLE_OPERATORS(BuiltinShader);

class VulkanShaderCache
{
public:
  VulkanShaderCache(WrappedVulkan *driver);
  ~VulkanShaderCache();

  rdcstr GetSPIRVBlob(const rdcspv::CompilationSettings &settings, const rdcstr &src,
                      SPIRVBlob &outBlob);

  SPIRVBlob GetBuiltinBlob(BuiltinShader builtin) { return m_BuiltinShaderBlobs[(size_t)builtin]; }
  VkShaderModule GetBuiltinModule(BuiltinShader builtin)
  {
    return m_BuiltinShaderModules[(size_t)builtin];
  }
  VkPipelineCache GetPipeCache() { return m_PipelineCache; }
  void MakeGraphicsPipelineInfo(VkGraphicsPipelineCreateInfo &pipeCreateInfo, ResourceId pipeline);
  void MakeComputePipelineInfo(VkComputePipelineCreateInfo &pipeCreateInfo, ResourceId pipeline);

  bool IsMS2ArraySupported() { return m_MS2ArraySupported; }
  bool IsArray2MSSupported() { return m_Array2MSSupported; }
  rdcstr GetGlobalDefines() { return m_GlobalDefines; }
  void SetCaching(bool enabled) { m_CacheShaders = enabled; }
private:
  static const uint32_t m_ShaderCacheMagic = 0xf00d00d5;
  static const uint32_t m_ShaderCacheVersion = 1;

  void GetPipeCacheBlob();
  void SetPipeCacheBlob(bytebuf &blob);

  WrappedVulkan *m_pDriver = NULL;
  VkDevice m_Device = VK_NULL_HANDLE;

  bytebuf m_PipeCacheBlob;
  VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

  rdcstr m_GlobalDefines;

  bool m_MS2ArraySupported = false, m_Array2MSSupported = false;

  bool m_ShaderCacheDirty = false, m_CacheShaders = false;
  std::map<uint32_t, SPIRVBlob> m_ShaderCache;

  SPIRVBlob m_BuiltinShaderBlobs[arraydim<BuiltinShader>()] = {NULL};
  VkShaderModule m_BuiltinShaderModules[arraydim<BuiltinShader>()] = {VK_NULL_HANDLE};
};
