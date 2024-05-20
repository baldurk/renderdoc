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

#include "vk_shader_cache.h"
#include "common/shader_cache.h"
#include "data/glsl_shaders.h"
#include "strings/string_utils.h"

enum class FeatureCheck
{
  NoCheck = 0x0,
  ShaderMSAAStorage = 0x1,
  FragmentStores = 0x2,
  NonMetalBackend = 0x4,
  FormatlessWrite = 0x8,
  SampleShading = 0x10,
};

BITMASK_OPERATORS(FeatureCheck);

enum class BuiltinShaderFlags
{
  None = 0x0,
  BaseTypeParameterised = 0x1,
  TextureTypeParameterised = 0x2,
};

BITMASK_OPERATORS(BuiltinShaderFlags);

struct BuiltinShaderConfig
{
  BuiltinShaderConfig(BuiltinShader builtin, EmbeddedResourceType resource,
                      rdcspv::ShaderStage stage, FeatureCheck checks = FeatureCheck::NoCheck,
                      BuiltinShaderFlags flags = BuiltinShaderFlags::None)
      : builtin(builtin), resource(resource), stage(stage), checks(checks), flags(flags)
  {
  }
  BuiltinShader builtin;
  EmbeddedResourceType resource;
  rdcspv::ShaderStage stage;
  FeatureCheck checks;
  BuiltinShaderFlags flags;
};

static const BuiltinShaderConfig builtinShaders[] = {
    BuiltinShaderConfig(BuiltinShader::BlitVS, EmbeddedResource(glsl_blit_vert),
                        rdcspv::ShaderStage::Vertex),
    BuiltinShaderConfig(BuiltinShader::CheckerboardFS, EmbeddedResource(glsl_checkerboard_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::TexDisplayFS, EmbeddedResource(glsl_texdisplay_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::FixedColFS, EmbeddedResource(glsl_fixedcol_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::TextVS, EmbeddedResource(glsl_vktext_vert),
                        rdcspv::ShaderStage::Vertex),
    BuiltinShaderConfig(BuiltinShader::TextFS, EmbeddedResource(glsl_vktext_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::MeshVS, EmbeddedResource(glsl_mesh_vert),
                        rdcspv::ShaderStage::Vertex),
    BuiltinShaderConfig(BuiltinShader::MeshGS, EmbeddedResource(glsl_mesh_geom),
                        rdcspv::ShaderStage::Geometry),
    BuiltinShaderConfig(BuiltinShader::MeshFS, EmbeddedResource(glsl_mesh_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::MeshCS, EmbeddedResource(glsl_mesh_comp),
                        rdcspv::ShaderStage::Compute),
    BuiltinShaderConfig(BuiltinShader::QuadResolveFS, EmbeddedResource(glsl_quadresolve_frag),
                        rdcspv::ShaderStage::Fragment, FeatureCheck::FragmentStores),
    BuiltinShaderConfig(BuiltinShader::QuadWriteFS, EmbeddedResource(glsl_quadwrite_frag),
                        rdcspv::ShaderStage::Fragment,
                        FeatureCheck::FragmentStores | FeatureCheck::NonMetalBackend),
    BuiltinShaderConfig(BuiltinShader::TrisizeGS, EmbeddedResource(glsl_trisize_geom),
                        rdcspv::ShaderStage::Geometry),
    BuiltinShaderConfig(BuiltinShader::TrisizeFS, EmbeddedResource(glsl_trisize_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::TexRemap, EmbeddedResource(glsl_texremap_frag),
                        rdcspv::ShaderStage::Fragment, FeatureCheck::NoCheck,
                        BuiltinShaderFlags::BaseTypeParameterised),
    BuiltinShaderConfig(BuiltinShader::PixelHistoryMSCopyCS,
                        EmbeddedResource(glsl_pixelhistory_mscopy_comp), rdcspv::ShaderStage::Compute),
    BuiltinShaderConfig(BuiltinShader::PixelHistoryMSCopyDepthCS,
                        EmbeddedResource(glsl_pixelhistory_mscopy_depth_comp),
                        rdcspv::ShaderStage::Compute),
    BuiltinShaderConfig(BuiltinShader::PixelHistoryPrimIDFS,
                        EmbeddedResource(glsl_pixelhistory_primid_frag),
                        rdcspv::ShaderStage::Fragment),
    BuiltinShaderConfig(BuiltinShader::ShaderDebugSampleVS,
                        EmbeddedResource(glsl_shaderdebug_sample_vert), rdcspv::ShaderStage::Vertex),
    BuiltinShaderConfig(BuiltinShader::DiscardFS, EmbeddedResource(glsl_discard_frag),
                        rdcspv::ShaderStage::Fragment, FeatureCheck::NoCheck,
                        BuiltinShaderFlags::BaseTypeParameterised),
    BuiltinShaderConfig(
        BuiltinShader::HistogramCS, EmbeddedResource(glsl_histogram_comp),
        rdcspv::ShaderStage::Compute, FeatureCheck::NoCheck,
        BuiltinShaderFlags::BaseTypeParameterised | BuiltinShaderFlags::TextureTypeParameterised),
    BuiltinShaderConfig(
        BuiltinShader::MinMaxTileCS, EmbeddedResource(glsl_minmaxtile_comp),
        rdcspv::ShaderStage::Compute, FeatureCheck::NoCheck,
        BuiltinShaderFlags::BaseTypeParameterised | BuiltinShaderFlags::TextureTypeParameterised),
    BuiltinShaderConfig(BuiltinShader::MinMaxResultCS, EmbeddedResource(glsl_minmaxresult_comp),
                        rdcspv::ShaderStage::Compute, FeatureCheck::NoCheck,
                        BuiltinShaderFlags::BaseTypeParameterised),
    BuiltinShaderConfig(BuiltinShader::MS2BufferCS, EmbeddedResource(glsl_vk_ms2buffer_comp),
                        rdcspv::ShaderStage::Compute, FeatureCheck::NonMetalBackend),
    BuiltinShaderConfig(BuiltinShader::DepthMS2BufferCS, EmbeddedResource(glsl_vk_depthms2buffer_comp),
                        rdcspv::ShaderStage::Compute, FeatureCheck::NonMetalBackend),
    BuiltinShaderConfig(BuiltinShader::Buffer2MSCS, EmbeddedResource(glsl_vk_buffer2ms_comp),
                        rdcspv::ShaderStage::Compute,
                        FeatureCheck::ShaderMSAAStorage | FeatureCheck::FormatlessWrite |
                            FeatureCheck::NonMetalBackend),
    BuiltinShaderConfig(BuiltinShader::DepthBuf2MSFS, EmbeddedResource(glsl_vk_depthbuf2ms_frag),
                        rdcspv::ShaderStage::Fragment,
                        FeatureCheck::SampleShading | FeatureCheck::NonMetalBackend),
    BuiltinShaderConfig(BuiltinShader::DepthCopyFS, EmbeddedResource(glsl_depth_copy_frag),
                        rdcspv::ShaderStage::Fragment, FeatureCheck::FragmentStores),
    BuiltinShaderConfig(BuiltinShader::DepthCopyMSFS, EmbeddedResource(glsl_depth_copyms_frag),
                        rdcspv::ShaderStage::Fragment, FeatureCheck::FragmentStores),
};

RDCCOMPILE_ASSERT(ARRAY_COUNT(builtinShaders) == arraydim<BuiltinShader>(),
                  "Missing built-in shader config");

static bool PassesChecks(const BuiltinShaderConfig &config, const VkDriverInfo &driverVersion,
                         const VkPhysicalDeviceFeatures &features)
{
  if(config.checks & FeatureCheck::ShaderMSAAStorage)
  {
    if(driverVersion.TexelFetchBrokenDriver() || driverVersion.AMDStorageMSAABrokenDriver() ||
       !features.shaderStorageImageMultisample)
    {
      return false;
    }
  }

  if(config.checks & FeatureCheck::FormatlessWrite)
  {
    if(!features.shaderStorageImageWriteWithoutFormat)
    {
      return false;
    }
  }

  if(config.checks & FeatureCheck::SampleShading)
  {
    if(!features.sampleRateShading)
    {
      return false;
    }
  }

  if(config.checks & FeatureCheck::FragmentStores)
  {
    if(!features.fragmentStoresAndAtomics)
      return false;
  }

  if(config.checks & FeatureCheck::NonMetalBackend)
  {
    // for now we don't allow it at all - in future we could check on whether it's been enabled
    // via a more advanced query
    if(driverVersion.RunningOnMetal())
      return false;
  }

  if(config.stage == rdcspv::ShaderStage::Geometry && !features.geometryShader)
    return false;

  return true;
}

struct VulkanBlobShaderCallbacks
{
  bool Create(uint32_t size, byte *data, SPIRVBlob *ret) const
  {
    RDCASSERT(ret);

    SPIRVBlob blob = new rdcarray<uint32_t>();

    blob->resize(size / sizeof(uint32_t));

    memcpy(&(*blob)[0], data, size);

    *ret = blob;

    return true;
  }

  void Destroy(SPIRVBlob blob) const { delete blob; }
  uint32_t GetSize(SPIRVBlob blob) const { return (uint32_t)(blob->size() * sizeof(uint32_t)); }
  const byte *GetData(SPIRVBlob blob) const { return (const byte *)blob->data(); }
} VulkanShaderCacheCallbacks;

struct VkPipeCacheHeader
{
  uint32_t length;
  VkPipelineCacheHeaderVersion version;
  uint32_t vendorID;
  uint32_t deviceID;
  byte uuid[VK_UUID_SIZE];
};

VulkanShaderCache::VulkanShaderCache(WrappedVulkan *driver)
{
  // Load shader cache, if present
  bool success = LoadShaderCache("vkshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, VulkanShaderCacheCallbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  m_pDriver = driver;
  m_Device = driver->GetDev();

  SetCaching(true);

  const VkDriverInfo &driverVersion = driver->GetDriverInfo();
  const VkPhysicalDeviceFeatures &enabledFeatures = driver->GetDeviceEnabledFeatures();
  const VkPhysicalDeviceFeatures &availFeatures = driver->GetDeviceAvailableFeatures();

  rdcstr globalDefines = "#define HAS_BIT_CONVERSION 1\n";
  if(driverVersion.TexelFetchBrokenDriver())
    globalDefines += "#define NO_TEXEL_FETCH\n";
  if(driverVersion.RunningOnMetal())
    globalDefines += "#define METAL_BACKEND\n";

  rdcspv::CompilationSettings compileSettings;
  compileSettings.lang = rdcspv::InputLanguage::VulkanGLSL;

  m_Buffer2MSSupported =
      PassesChecks(builtinShaders[(size_t)BuiltinShader::Buffer2MSCS], driverVersion, availFeatures);

  for(auto i : indices<BuiltinShader>())
  {
    const BuiltinShaderConfig &config = builtinShaders[i];

    RDCASSERT(config.builtin == (BuiltinShader)i);

    bool passesChecks = PassesChecks(config, driverVersion, enabledFeatures);

    if(!passesChecks)
      continue;

    size_t baseTypeCount = size_t(BuiltinShaderBaseType::First) + 1,
           textureTypeCount = size_t(BuiltinShaderTextureType::First) + 1;

    if(config.flags & BuiltinShaderFlags::BaseTypeParameterised)
      baseTypeCount = (size_t)BuiltinShaderBaseType::Count;
    if(config.flags & BuiltinShaderFlags::TextureTypeParameterised)
      textureTypeCount = (size_t)BuiltinShaderTextureType::Count;

    compileSettings.stage = config.stage;

    // for shaders that aren't parameterised these loops will be a no-op that only iterates once,
    // and fills in [First][First] entry.
    for(size_t baseType = (size_t)BuiltinShaderBaseType::First; baseType < baseTypeCount; baseType++)
    {
      for(size_t textureType = (size_t)BuiltinShaderTextureType::First;
          textureType < textureTypeCount; textureType++)
      {
        rdcstr defines = globalDefines;

        defines += rdcstr("#define SHADER_RESTYPE ") + ToStr(textureType) + "\n";
        defines += rdcstr("#define SHADER_BASETYPE ") + ToStr(baseType) + "\n";

        SPIRVBlob &blob = m_BuiltinShaderBlobs[i][baseType][textureType];
        rdcstr source = GetDynamicEmbeddedResource(config.resource);

        uint32_t inputHash = strhash(source.c_str());
        inputHash = strhash(defines.c_str(), inputHash);

        // bump this version if anything inside GenerateGLSLShader changes. This is used to
        // determine if we can skip the call to GenerateGLSLShader (which calls out to glslang).
        // Otherwise we'll use the cached SPIR-V generated by the previous call using the same
        // source & defines.
        inputHash = strhash("inputHashVersion1", inputHash);

        rdcstr err;

        if(m_ShaderCache.find(inputHash) != m_ShaderCache.end())
          blob = m_ShaderCache[inputHash];

        if(blob == NULL)
        {
          err = GetSPIRVBlob(compileSettings,
                             GenerateGLSLShader(source, ShaderType::Vulkan, 430, defines), blob);

          // if we missed the inputHash, make a copy there too.
          if(m_CacheShaders && blob)
          {
            m_ShaderCache[inputHash] = new rdcarray<uint32_t>(*blob);
            m_ShaderCacheDirty = true;
          }
        }

        if(!err.empty() || blob == VK_NULL_HANDLE)
        {
          RDCERR("Error compiling builtin %u (baseType %zu textureType %zu): %s", (uint32_t)i,
                 baseType, textureType, err.c_str());
        }
        else
        {
          VkShaderModuleCreateInfo modinfo = {
              VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              NULL,
              0,
              blob->size() * sizeof(uint32_t),
              blob->data(),
          };

          VkResult vkr = driver->vkCreateShaderModule(
              m_Device, &modinfo, NULL, &m_BuiltinShaderModules[i][baseType][textureType]);
          driver->CheckVkResult(vkr);

          driver->GetResourceManager()->SetInternalResource(
              GetResID(m_BuiltinShaderModules[i][baseType][textureType]));
        }
      }
    }
  }

  {
    VkPipelineCacheCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};

    GetPipeCacheBlob();

    if(!m_PipeCacheBlob.empty())
    {
      if(m_PipeCacheBlob.size() < sizeof(VkPipeCacheHeader))
      {
        m_PipeCacheBlob.clear();
      }
      else
      {
        VkPipeCacheHeader *header = (VkPipeCacheHeader *)m_PipeCacheBlob.data();

        // check explicitly for incompatibility
        if(header->length != sizeof(VkPipeCacheHeader))
        {
          m_PipeCacheBlob.clear();
          RDCLOG("Pipeline cache header length %u is unexpected, not using cache", header->length);
        }
        else if(header->version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
        {
          m_PipeCacheBlob.clear();
          RDCLOG("Pipeline cache header version %u is unexpected, not using cache", header->version);
        }
        else if(header->vendorID != m_pDriver->GetDeviceProps().vendorID)
        {
          m_PipeCacheBlob.clear();
          RDCLOG("Pipeline cache header vendorID %u doesn't match %u", header->vendorID,
                 m_pDriver->GetDeviceProps().vendorID);
        }
        else if(header->deviceID != m_pDriver->GetDeviceProps().deviceID)
        {
          m_PipeCacheBlob.clear();
          RDCLOG("Pipeline cache header deviceID %u doesn't match %u", header->deviceID,
                 m_pDriver->GetDeviceProps().deviceID);
        }
        else if(memcmp(header->uuid, m_pDriver->GetDeviceProps().pipelineCacheUUID, VK_UUID_SIZE) != 0)
        {
          m_PipeCacheBlob.clear();
          RDCLOG("Pipeline cache UUID doesn't match");
        }
      }
    }

    if(!m_PipeCacheBlob.empty())
    {
      createInfo.initialDataSize = m_PipeCacheBlob.size();
      createInfo.pInitialData = m_PipeCacheBlob.data();
    }

    // manually wrap the cache so the data is uploaded

    VkResult vkr = ObjDisp(m_Device)->CreatePipelineCache(Unwrap(m_Device), &createInfo, NULL,
                                                          &m_PipelineCache);
    driver->CheckVkResult(vkr);

    if(vkr == VK_SUCCESS)
    {
      ResourceId id =
          m_pDriver->GetResourceManager()->WrapResource(Unwrap(m_Device), m_PipelineCache);

      if(IsCaptureMode(m_pDriver->GetState()))
      {
        m_pDriver->GetResourceManager()->AddResourceRecord(m_PipelineCache);
      }
      else
      {
        m_pDriver->GetResourceManager()->AddLiveResource(id, m_PipelineCache);
      }
    }
  }

  SetCaching(false);
}

VulkanShaderCache::~VulkanShaderCache()
{
  if(m_PipelineCache != VK_NULL_HANDLE)
  {
    bytebuf blob;
    size_t size = 0;
    ObjDisp(m_Device)->GetPipelineCacheData(Unwrap(m_Device), Unwrap(m_PipelineCache), &size, NULL);
    blob.resize(size);
    ObjDisp(m_Device)->GetPipelineCacheData(Unwrap(m_Device), Unwrap(m_PipelineCache), &size,
                                            blob.data());
    SetPipeCacheBlob(blob);
    m_pDriver->vkDestroyPipelineCache(m_Device, m_PipelineCache, NULL);
  }

  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("vkshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    VulkanShaderCacheCallbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      VulkanShaderCacheCallbacks.Destroy(it->second);
  }

  for(size_t i = 0; i < ARRAY_COUNT(m_BuiltinShaderModules); i++)
    for(size_t b = 0; b < ARRAY_COUNT(m_BuiltinShaderModules[0]); b++)
      for(size_t t = 0; t < ARRAY_COUNT(m_BuiltinShaderModules[0][0]); t++)
        m_pDriver->vkDestroyShaderModule(m_Device, m_BuiltinShaderModules[i][b][t], NULL);
}

rdcstr VulkanShaderCache::GetSPIRVBlob(const rdcspv::CompilationSettings &settings,
                                       const rdcstr &src, SPIRVBlob &outBlob)
{
  RDCASSERT(!src.empty());

  uint32_t hash = strhash(src.c_str());

  char typestr[3] = {'a', 'a', 0};
  typestr[0] += (char)settings.stage;
  typestr[1] += (char)settings.lang;
  hash = strhash(typestr, hash);

  if(m_ShaderCache.find(hash) != m_ShaderCache.end())
  {
    outBlob = m_ShaderCache[hash];
    return "";
  }

  SPIRVBlob spirv = new rdcarray<uint32_t>();
  rdcstr errors = rdcspv::Compile(settings, {src}, *spirv);

  if(!errors.empty())
  {
    rdcstr logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCWARN("Shader compile error:\n%s", logerror.c_str());

    delete spirv;
    outBlob = NULL;
    return errors;
  }

  outBlob = spirv;

  if(m_CacheShaders)
  {
    m_ShaderCache[hash] = spirv;
    m_ShaderCacheDirty = true;
  }

  return errors;
}

void VulkanShaderCache::GetPipeCacheBlob()
{
  m_PipeCacheBlob.clear();

  uint32_t hash = strhash(StringFormat::Fmt("PipelineCache%x%x", m_pDriver->GetDeviceProps().vendorID,
                                            m_pDriver->GetDeviceProps().deviceID)
                              .c_str());

  auto it = m_ShaderCache.find(hash);

  if(it != m_ShaderCache.end())
  {
    SPIRVBlob blob = it->second;

    // first uint32_t is the real byte size, since we rounded up to the nearest uint32 to store in a
    // SPIRVBlob
    uint32_t size = blob->at(0);
    if(size == rdcspv::MagicNumber)
    {
      RDCLOG("Hash collision - pipeline cache trampled");
      return;
    }
    m_PipeCacheBlob.resize(size);
    memcpy(m_PipeCacheBlob.data(), blob->data() + 1, m_PipeCacheBlob.size());
  }
}

void VulkanShaderCache::SetPipeCacheBlob(bytebuf &blob)
{
  if(m_PipeCacheBlob == blob)
    return;

  VkPipeCacheHeader *header = (VkPipeCacheHeader *)blob.data();

  uint32_t hash =
      strhash(StringFormat::Fmt("PipelineCache%x%x", header->vendorID, header->deviceID).c_str());

  rdcarray<uint32_t> *spirvBlob = new rdcarray<uint32_t>();

  // align the size up to the nearest 4, and add one extra for us to store the real byte size
  spirvBlob->resize(AlignUp4(blob.size()) / 4 + 1);

  // store the size, then the real data after that
  (*spirvBlob)[0] = (uint32_t)blob.size();
  memcpy(spirvBlob->data() + 1, blob.data(), blob.size());

  m_ShaderCache[hash] = spirvBlob;
  m_ShaderCacheDirty = true;
}

void VulkanShaderCache::MakeGraphicsPipelineInfo(VkGraphicsPipelineCreateInfo &pipeCreateInfo,
                                                 ResourceId pipeline)
{
  // skip if invalid pipeline
  if(pipeline == ResourceId())
  {
    RDCEraseEl(pipeCreateInfo);
    return;
  }

  const VulkanCreationInfo::Pipeline &pipeInfo = m_pDriver->m_CreationInfo.m_Pipeline[pipeline];

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  static VkPipelineShaderStageCreateInfo stages[NumShaderStages];
  static VkSpecializationInfo specInfo[NumShaderStages];
  static rdcarray<VkSpecializationMapEntry> specMapEntries;

  // the specialization constants can't use more than a uint64_t, so we just over-allocate
  static rdcarray<uint64_t> specdata;

  size_t specEntries = 0;

  for(uint32_t i = 0; i < NumShaderStages; i++)
    specEntries += pipeInfo.shaders[i].specialization.size();

  specMapEntries.resize(specEntries);
  specdata.resize(specEntries);

  VkSpecializationMapEntry *entry = specMapEntries.data();
  uint64_t *data = specdata.data();

  uint32_t stageCount = 0;
  uint32_t dataOffset = 0;

  static VkPipelineShaderStageRequiredSubgroupSizeCreateInfo reqSubgroupSize[NumShaderStages] = {};

  // reserve space for spec constants
  for(uint32_t i = 0; i < NumShaderStages; i++)
  {
    if(pipeInfo.shaders[i].module != ResourceId())
    {
      stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[stageCount].stage = (VkShaderStageFlagBits)(1 << i);
      stages[stageCount].module = rm->GetCurrentHandle<VkShaderModule>(pipeInfo.shaders[i].module);
      stages[stageCount].pName = pipeInfo.shaders[i].entryPoint.c_str();
      stages[stageCount].pNext = NULL;
      stages[stageCount].pSpecializationInfo = NULL;

      if(pipeInfo.shaders[i].requiredSubgroupSize != 0)
      {
        reqSubgroupSize[i].sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO;
        reqSubgroupSize[i].requiredSubgroupSize = pipeInfo.shaders[i].requiredSubgroupSize;
        stages[stageCount].pNext = &reqSubgroupSize[i];
      }

      if(!pipeInfo.shaders[i].specialization.empty())
      {
        stages[stageCount].pSpecializationInfo = &specInfo[i];
        specInfo[i].pMapEntries = entry;
        specInfo[i].mapEntryCount = (uint32_t)pipeInfo.shaders[i].specialization.size();

        for(size_t s = 0; s < pipeInfo.shaders[i].specialization.size(); s++)
        {
          entry[s].constantID = pipeInfo.shaders[i].specialization[s].specID;
          entry[s].size = pipeInfo.shaders[i].specialization[s].dataSize;
          entry[s].offset = dataOffset * sizeof(uint64_t);

          data[dataOffset] = pipeInfo.shaders[i].specialization[s].value;

          dataOffset++;
        }

        specInfo[i].dataSize = specdata.size() * sizeof(uint64_t);
        specInfo[i].pData = specdata.data();

        entry += specInfo[i].mapEntryCount;
      }

      stageCount++;
    }
  }

  static VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  vi.pNext = NULL;

  static VkVertexInputAttributeDescription viattr[128] = {};
  static VkVertexInputBindingDescription vibind[128] = {};

  vi.pVertexAttributeDescriptions = viattr;
  vi.pVertexBindingDescriptions = vibind;

  vi.vertexAttributeDescriptionCount = (uint32_t)pipeInfo.vertexAttrs.size();
  vi.vertexBindingDescriptionCount = (uint32_t)pipeInfo.vertexBindings.size();

  for(uint32_t i = 0; i < vi.vertexAttributeDescriptionCount; i++)
  {
    viattr[i].binding = pipeInfo.vertexAttrs[i].binding;
    viattr[i].offset = pipeInfo.vertexAttrs[i].byteoffset;
    viattr[i].format = pipeInfo.vertexAttrs[i].format;
    viattr[i].location = pipeInfo.vertexAttrs[i].location;
  }

  for(uint32_t i = 0; i < vi.vertexBindingDescriptionCount; i++)
  {
    vibind[i].binding = pipeInfo.vertexBindings[i].vbufferBinding;
    vibind[i].stride = pipeInfo.vertexBindings[i].bytestride;
    vibind[i].inputRate = pipeInfo.vertexBindings[i].perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                                 : VK_VERTEX_INPUT_RATE_VERTEX;
  }

  static VkPipelineVertexInputDivisorStateCreateInfoKHR vertexDivisor = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR,
  };
  static VkVertexInputBindingDivisorDescriptionKHR vibindDivisors[128] = {};

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_vertex_attribute_divisor ||
     m_pDriver->GetExtensions(GetRecord(m_Device)).ext_KHR_vertex_attribute_divisor)
  {
    vertexDivisor.pVertexBindingDivisors = vibindDivisors;
    vertexDivisor.vertexBindingDivisorCount = 0;

    for(uint32_t i = 0; i < vi.vertexBindingDescriptionCount; i++)
    {
      if(pipeInfo.vertexBindings[i].perInstance)
      {
        uint32_t instIdx = vertexDivisor.vertexBindingDivisorCount++;
        vibindDivisors[instIdx].binding = pipeInfo.vertexBindings[i].vbufferBinding;
        vibindDivisors[instIdx].divisor = pipeInfo.vertexBindings[i].instanceDivisor;
      }
    }

    if(vertexDivisor.vertexBindingDivisorCount > 0)
    {
      vertexDivisor.pNext = vi.pNext;
      vi.pNext = &vertexDivisor;
    }
  }

  RDCASSERT(ARRAY_COUNT(viattr) >= pipeInfo.vertexAttrs.size());
  RDCASSERT(ARRAY_COUNT(vibind) >= pipeInfo.vertexBindings.size());

  static VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

  ia.pNext = NULL;

  ia.topology = pipeInfo.topology;
  ia.primitiveRestartEnable = pipeInfo.primitiveRestartEnable;

  static VkPipelineTessellationStateCreateInfo tess = {
      VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

  tess.pNext = NULL;

  tess.patchControlPoints = pipeInfo.patchControlPoints;

  static VkPipelineTessellationDomainOriginStateCreateInfo tessDomain = {
      VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_KHR_maintenance2)
  {
    tessDomain.domainOrigin = pipeInfo.tessellationDomainOrigin;

    tessDomain.pNext = tess.pNext;
    tess.pNext = &tessDomain;
  }

  static VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  static VkViewport views[32] = {};
  static VkRect2D scissors[32] = {};

  vp.pNext = NULL;

  memcpy(views, &pipeInfo.viewports[0], pipeInfo.viewports.size() * sizeof(VkViewport));

  vp.pViewports = &views[0];
  vp.viewportCount = (uint32_t)pipeInfo.viewports.size();

  memcpy(scissors, &pipeInfo.scissors[0], pipeInfo.scissors.size() * sizeof(VkRect2D));

  vp.pScissors = &scissors[0];
  vp.scissorCount = (uint32_t)pipeInfo.scissors.size();

  RDCASSERT(ARRAY_COUNT(views) >= pipeInfo.viewports.size());
  RDCASSERT(ARRAY_COUNT(scissors) >= pipeInfo.scissors.size());

  static VkPipelineViewportDepthClipControlCreateInfoEXT depthClipControl = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_depth_clip_control)
  {
    depthClipControl.negativeOneToOne = pipeInfo.negativeOneToOne;

    depthClipControl.pNext = vp.pNext;
    vp.pNext = &depthClipControl;
  }

  static VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
  };

  rs.pNext = NULL;

  rs.depthClampEnable = pipeInfo.depthClampEnable;
  rs.rasterizerDiscardEnable = pipeInfo.rasterizerDiscardEnable;
  rs.polygonMode = pipeInfo.polygonMode;
  rs.cullMode = pipeInfo.cullMode;
  rs.frontFace = pipeInfo.frontFace;
  rs.depthBiasEnable = pipeInfo.depthBiasEnable;
  rs.depthBiasConstantFactor = pipeInfo.depthBiasConstantFactor;
  rs.depthBiasClamp = pipeInfo.depthBiasClamp;
  rs.depthBiasSlopeFactor = pipeInfo.depthBiasSlopeFactor;
  rs.lineWidth = pipeInfo.lineWidth;

  static VkPipelineRasterizationConservativeStateCreateInfoEXT conservRast = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_conservative_rasterization)
  {
    conservRast.conservativeRasterizationMode = pipeInfo.conservativeRasterizationMode;
    conservRast.extraPrimitiveOverestimationSize = pipeInfo.extraPrimitiveOverestimationSize;

    conservRast.pNext = rs.pNext;
    rs.pNext = &conservRast;
  }

  static VkPipelineRasterizationStateStreamCreateInfoEXT rastStream = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_transform_feedback &&
     pipeInfo.rasterizationStream != 0)
  {
    rastStream.rasterizationStream = pipeInfo.rasterizationStream;

    rastStream.pNext = rs.pNext;
    rs.pNext = &rastStream;
  }

  static VkPipelineRasterizationDepthClipStateCreateInfoEXT depthClipState = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_depth_clip_enable)
  {
    depthClipState.depthClipEnable = pipeInfo.depthClipEnable;

    depthClipState.pNext = rs.pNext;
    rs.pNext = &depthClipState;
  }

  static VkPipelineRasterizationLineStateCreateInfoEXT lineRasterState = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_line_rasterization)
  {
    lineRasterState.lineRasterizationMode = pipeInfo.lineRasterMode;
    lineRasterState.stippledLineEnable = pipeInfo.stippleEnabled;
    lineRasterState.lineStippleFactor = pipeInfo.stippleFactor;
    lineRasterState.lineStipplePattern = pipeInfo.stipplePattern;

    lineRasterState.pNext = rs.pNext;
    rs.pNext = &lineRasterState;
  }

  static VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
  };

  msaa.pNext = NULL;

  msaa.rasterizationSamples = pipeInfo.rasterizationSamples;
  msaa.sampleShadingEnable = pipeInfo.sampleShadingEnable;
  msaa.minSampleShading = pipeInfo.minSampleShading;
  msaa.pSampleMask = &pipeInfo.sampleMask;
  msaa.alphaToCoverageEnable = pipeInfo.alphaToCoverageEnable;
  msaa.alphaToOneEnable = pipeInfo.alphaToOneEnable;

  static VkPipelineSampleLocationsStateCreateInfoEXT sampleLoc = {
      VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_sample_locations &&
     pipeInfo.sampleLocations.enabled)
  {
    sampleLoc.sampleLocationsEnable = pipeInfo.sampleLocations.enabled;
    sampleLoc.sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
    sampleLoc.sampleLocationsInfo.sampleLocationGridSize = pipeInfo.sampleLocations.gridSize;
    sampleLoc.sampleLocationsInfo.sampleLocationsPerPixel = pipeInfo.rasterizationSamples;
    sampleLoc.sampleLocationsInfo.sampleLocationsCount =
        (uint32_t)pipeInfo.sampleLocations.locations.size();
    sampleLoc.sampleLocationsInfo.pSampleLocations = pipeInfo.sampleLocations.locations.data();

    sampleLoc.pNext = msaa.pNext;
    msaa.pNext = &sampleLoc;
  }

  static VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

  ds.pNext = NULL;

  ds.depthTestEnable = pipeInfo.depthTestEnable;
  ds.depthWriteEnable = pipeInfo.depthWriteEnable;
  ds.depthCompareOp = pipeInfo.depthCompareOp;
  ds.depthBoundsTestEnable = pipeInfo.depthBoundsEnable;
  ds.stencilTestEnable = pipeInfo.stencilTestEnable;
  ds.front = pipeInfo.front;
  ds.back = pipeInfo.back;
  ds.minDepthBounds = pipeInfo.minDepthBounds;
  ds.maxDepthBounds = pipeInfo.maxDepthBounds;

  static VkPipelineColorBlendStateCreateInfo cb = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};

  cb.pNext = NULL;

  cb.logicOpEnable = pipeInfo.logicOpEnable;
  cb.logicOp = pipeInfo.logicOp;
  memcpy(cb.blendConstants, pipeInfo.blendConst, sizeof(cb.blendConstants));

  static VkPipelineColorBlendAttachmentState atts[32] = {};

  cb.attachmentCount = (uint32_t)pipeInfo.attachments.size();
  cb.pAttachments = atts;

  for(uint32_t i = 0; i < cb.attachmentCount; i++)
  {
    atts[i].blendEnable = pipeInfo.attachments[i].blendEnable;
    atts[i].colorWriteMask = pipeInfo.attachments[i].channelWriteMask;
    atts[i].alphaBlendOp = pipeInfo.attachments[i].alphaBlend.Operation;
    atts[i].srcAlphaBlendFactor = pipeInfo.attachments[i].alphaBlend.Source;
    atts[i].dstAlphaBlendFactor = pipeInfo.attachments[i].alphaBlend.Destination;
    atts[i].colorBlendOp = pipeInfo.attachments[i].blend.Operation;
    atts[i].srcColorBlendFactor = pipeInfo.attachments[i].blend.Source;
    atts[i].dstColorBlendFactor = pipeInfo.attachments[i].blend.Destination;
  }

  RDCASSERT(ARRAY_COUNT(atts) >= pipeInfo.attachments.size());

  static VkDynamicState dynSt[VkDynamicCount];

  static VkPipelineDynamicStateCreateInfo dyn = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

  dyn.pNext = NULL;

  dyn.dynamicStateCount = 0;
  dyn.pDynamicStates = dynSt;

  for(uint32_t i = 0; i < VkDynamicCount; i++)
    if(pipeInfo.dynamicStates[i])
      dynSt[dyn.dynamicStateCount++] = ConvertDynamicState((VulkanDynamicStateIndex)i);

  // since we don't have to worry about threading, we point everything at the above static structs

  VkGraphicsPipelineCreateInfo ret = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      NULL,
      pipeInfo.flags,
      stageCount,
      stages,
      &vi,
      &ia,
      &tess,
      &vp,
      &rs,
      &msaa,
      &ds,
      &cb,
      &dyn,
      VK_NULL_HANDLE,
      rm->GetCurrentHandle<VkRenderPass>(pipeInfo.renderpass),
      pipeInfo.subpass,
      VK_NULL_HANDLE,    // base pipeline handle
      0,                 // base pipeline index
  };

  // if the layouts are the same object (non-library case) we can just use it directly
  if(pipeInfo.vertLayout == pipeInfo.fragLayout)
  {
    ret.layout = rm->GetCurrentHandle<VkPipelineLayout>(pipeInfo.vertLayout);
  }
  else
  {
    ret.layout = m_CombinedPipeLayouts[pipeline];
    if(ret.layout == VK_NULL_HANDLE)
    {
      rdcarray<VkDescriptorSetLayout> descSetLayouts;

      for(ResourceId setLayout : pipeInfo.descSetLayouts)
        descSetLayouts.push_back(rm->GetCurrentHandle<VkDescriptorSetLayout>(setLayout));

      // don't have to handle separate vert/frag layouts as push constant ranges must be identical
      const VulkanCreationInfo::PipelineLayout &pipeLayoutInfo =
          m_pDriver->m_CreationInfo.m_PipelineLayout[pipeInfo.vertLayout];
      const rdcarray<VkPushConstantRange> &push = pipeLayoutInfo.pushRanges;

      VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = {
          VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          NULL,
          pipeLayoutInfo.flags,
          (uint32_t)descSetLayouts.size(),
          descSetLayouts.data(),
          (uint32_t)push.size(),
          push.data(),
      };

      VkResult vkr = m_pDriver->vkCreatePipelineLayout(m_pDriver->GetDev(), &pipeLayoutCreateInfo,
                                                       NULL, &m_CombinedPipeLayouts[pipeline]);
      m_pDriver->CheckVkResult(vkr);

      ret.layout = m_CombinedPipeLayouts[pipeline];
    }
  }

  static VkFormat colFormats[16] = {};
  static VkPipelineRenderingCreateInfo dynRenderCreate = {
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, NULL, pipeInfo.viewMask, 0, colFormats,
  };

  if(pipeInfo.renderpass == ResourceId())
  {
    dynRenderCreate.depthAttachmentFormat = pipeInfo.depthFormat;
    dynRenderCreate.stencilAttachmentFormat = pipeInfo.stencilFormat;
    dynRenderCreate.colorAttachmentCount = (uint32_t)pipeInfo.colorFormats.size();
    memcpy(colFormats, pipeInfo.colorFormats.data(), pipeInfo.colorFormats.byteSize());

    dynRenderCreate.pNext = ret.pNext;
    ret.pNext = &dynRenderCreate;
  }

  static VkPipelineDiscardRectangleStateCreateInfoEXT discardRects = {
      VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_discard_rectangles)
  {
    discardRects.discardRectangleMode = pipeInfo.discardMode;
    discardRects.discardRectangleCount = (uint32_t)pipeInfo.discardRectangles.size();
    discardRects.pDiscardRectangles = pipeInfo.discardRectangles.data();

    discardRects.pNext = ret.pNext;
    ret.pNext = &discardRects;
  }

  static VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRate = {
      VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,
  };

  if(pipeInfo.shadingRate.width != 1 || pipeInfo.shadingRate.height != 1 ||
     pipeInfo.shadingRateCombiners[0] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR ||
     pipeInfo.shadingRateCombiners[1] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR)
  {
    shadingRate.fragmentSize = pipeInfo.shadingRate;
    shadingRate.combinerOps[0] = pipeInfo.shadingRateCombiners[0];
    shadingRate.combinerOps[1] = pipeInfo.shadingRateCombiners[1];

    shadingRate.pNext = ret.pNext;
    ret.pNext = &shadingRate;
  }

  static VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provokeSetup = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,
  };

  if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_EXT_provoking_vertex)
  {
    provokeSetup.provokingVertexMode = pipeInfo.provokingVertex;

    provokeSetup.pNext = rs.pNext;
    rs.pNext = &provokeSetup;
  }

  // never create derivatives
  ret.flags &= ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;

  ret.flags &= ~VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
  ret.flags &= ~VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

  pipeCreateInfo = ret;
}

void VulkanShaderCache::MakeComputePipelineInfo(VkComputePipelineCreateInfo &pipeCreateInfo,
                                                ResourceId pipeline)
{
  const VulkanCreationInfo::Pipeline &pipeInfo = m_pDriver->m_CreationInfo.m_Pipeline[pipeline];

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  VkPipelineShaderStageCreateInfo stage;    // Returned by value
  static VkSpecializationInfo specInfo;
  static rdcarray<VkSpecializationMapEntry> specMapEntries;

  // the specialization constants can't use more than a uint64_t, so we just over-allocate
  static rdcarray<uint64_t> specdata;

  const uint32_t i = 5;    // Compute stage
  RDCASSERT(pipeInfo.shaders[i].module != ResourceId());

  size_t specEntries = pipeInfo.shaders[i].specialization.size();

  specdata.resize(specEntries);
  specMapEntries.resize(specEntries);
  VkSpecializationMapEntry *entry = specMapEntries.data();

  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = (VkShaderStageFlagBits)(1 << i);
  stage.module = rm->GetCurrentHandle<VkShaderModule>(pipeInfo.shaders[i].module);
  stage.pName = pipeInfo.shaders[i].entryPoint.c_str();
  stage.pNext = NULL;
  stage.pSpecializationInfo = NULL;
  stage.flags = 0;

  uint32_t dataOffset = 0;

  if(!pipeInfo.shaders[i].specialization.empty())
  {
    stage.pSpecializationInfo = &specInfo;
    specInfo.pMapEntries = entry;
    specInfo.mapEntryCount = (uint32_t)pipeInfo.shaders[i].specialization.size();

    for(size_t s = 0; s < pipeInfo.shaders[i].specialization.size(); s++)
    {
      entry[s].constantID = pipeInfo.shaders[i].specialization[s].specID;
      entry[s].size = pipeInfo.shaders[i].specialization[s].dataSize;
      entry[s].offset = dataOffset * sizeof(uint64_t);

      specdata[dataOffset] = pipeInfo.shaders[i].specialization[s].value;

      dataOffset++;
    }

    specInfo.dataSize = specdata.size() * sizeof(uint64_t);
    specInfo.pData = specdata.data();
  }

  static VkPipelineShaderStageRequiredSubgroupSizeCreateInfo reqSubgroupSize = {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
  };

  if(pipeInfo.shaders[i].requiredSubgroupSize != 0)
  {
    reqSubgroupSize.requiredSubgroupSize = pipeInfo.shaders[i].requiredSubgroupSize;
    stage.pNext = &reqSubgroupSize;
  }

  VkComputePipelineCreateInfo ret = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      NULL,
      pipeInfo.flags,
      stage,
      rm->GetCurrentHandle<VkPipelineLayout>(pipeInfo.compLayout),
      VK_NULL_HANDLE,    // base pipeline handle
      0,                 // base pipeline index
  };

  // never create derivatives
  ret.flags &= ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;

  pipeCreateInfo = ret;
}

void VulkanShaderCache::MakeShaderObjectInfo(VkShaderCreateInfoEXT &shadCreateInfo,
                                             ResourceId shaderObj)
{
  // skip if invalid shader object
  if(shaderObj == ResourceId())
  {
    RDCEraseEl(shadCreateInfo);
    return;
  }

  const VulkanCreationInfo::ShaderObject &shadInfo =
      m_pDriver->m_CreationInfo.m_ShaderObject[shaderObj];

  const VulkanCreationInfo::ShaderModule &moduleInfo =
      m_pDriver->m_CreationInfo.m_ShaderModule[shadInfo.shad.module];
  const rdcarray<uint32_t> &modSpirv = moduleInfo.spirv.GetSPIRV();

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  static VkSpecializationInfo specInfo;
  static rdcarray<VkSpecializationMapEntry> specMapEntries;

  // the specialization constants can't use more than a uint64_t, so we just over-allocate
  static rdcarray<uint64_t> specdata;

  size_t specEntries = shadInfo.shad.specialization.size();

  specdata.resize(specEntries);
  specMapEntries.resize(specEntries);
  VkSpecializationMapEntry *entry = specMapEntries.data();

  uint32_t stage = (uint32_t)shadInfo.shad.stage;

  static rdcarray<VkDescriptorSetLayout> descSetLayouts;
  descSetLayouts = {};

  for(ResourceId setLayout : shadInfo.descSetLayouts)
    descSetLayouts.push_back(rm->GetCurrentHandle<VkDescriptorSetLayout>(setLayout));

  VkShaderCreateInfoEXT ret = {VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                               NULL,
                               shadInfo.flags,
                               (VkShaderStageFlagBits)(1 << stage),
                               shadInfo.nextStage,
                               VK_SHADER_CODE_TYPE_SPIRV_EXT,
                               modSpirv.size() * sizeof(uint32_t),
                               modSpirv.data(),
                               shadInfo.shad.entryPoint.c_str(),
                               (uint32_t)descSetLayouts.size(),
                               descSetLayouts.data(),
                               (uint32_t)shadInfo.pushRanges.size(),
                               shadInfo.pushRanges.data(),
                               NULL};

  // specialization info
  uint32_t dataOffset = 0;

  if(!shadInfo.shad.specialization.empty())
  {
    ret.pSpecializationInfo = &specInfo;
    specInfo.pMapEntries = entry;
    specInfo.mapEntryCount = (uint32_t)shadInfo.shad.specialization.size();

    for(size_t s = 0; s < shadInfo.shad.specialization.size(); s++)
    {
      entry[s].constantID = shadInfo.shad.specialization[s].specID;
      entry[s].size = shadInfo.shad.specialization[s].dataSize;
      entry[s].offset = dataOffset * sizeof(uint64_t);

      specdata[dataOffset] = shadInfo.shad.specialization[s].value;

      dataOffset++;
    }

    specInfo.dataSize = specdata.size() * sizeof(uint64_t);
    specInfo.pData = specdata.data();
  }

  shadCreateInfo = ret;
}
