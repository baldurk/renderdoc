/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "vk_debug.h"
#include <float.h>
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "3rdparty/stb/stb_truetype.h"
#include "common/shader_cache.h"
#include "data/glsl_shaders.h"
#include "driver/shaders/spirv/spirv_common.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "serialise/string_utils.h"
#include "vk_core.h"

#define VULKAN 1
#include "data/glsl/debuguniforms.h"

const VkDeviceSize STAGE_BUFFER_BYTE_SIZE = 16 * 1024 * 1024ULL;

void VulkanDebugManager::GPUBuffer::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size,
                                           uint32_t ringSize, uint32_t flags)
{
  m_pDriver = driver;
  device = dev;

  align = (VkDeviceSize)driver->GetDeviceProps().limits.minUniformBufferOffsetAlignment;

  sz = size;
  // offset must be aligned, so ensure we have at least ringSize
  // copies accounting for that
  totalsize = ringSize == 1 ? size : AlignUp(size, align) * ringSize;
  curoffset = 0;

  ringCount = ringSize;

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, totalsize, 0,
  };

  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  if(flags & eGPUBufferVBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if(flags & eGPUBufferSSBO)
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  VkResult vkr = driver->vkCreateBuffer(dev, &bufInfo, NULL, &buf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {};
  driver->vkGetBufferMemoryRequirements(dev, buf, &mrq);

  VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size, 0};

  if(flags & eGPUBufferReadback)
    allocInfo.memoryTypeIndex = driver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  else if(flags & eGPUBufferGPULocal)
    allocInfo.memoryTypeIndex = driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits);
  else
    allocInfo.memoryTypeIndex = driver->GetUploadMemoryIndex(mrq.memoryTypeBits);

  vkr = driver->vkAllocateMemory(dev, &allocInfo, NULL, &mem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = driver->vkBindBufferMemory(dev, buf, mem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanDebugManager::GPUBuffer::FillDescriptor(VkDescriptorBufferInfo &desc)
{
  desc.buffer = Unwrap(buf);
  desc.offset = 0;
  desc.range = sz;
}

void VulkanDebugManager::GPUBuffer::Destroy()
{
  m_pDriver->vkDestroyBuffer(device, buf, NULL);
  m_pDriver->vkFreeMemory(device, mem, NULL);
}

void *VulkanDebugManager::GPUBuffer::Map(uint32_t *bindoffset, VkDeviceSize usedsize)
{
  VkDeviceSize offset = bindoffset ? curoffset : 0;
  VkDeviceSize size = usedsize > 0 ? usedsize : sz;

  // wrap around the ring, assuming the ring is large enough
  // that this memory is now free
  if(offset + sz > totalsize)
    offset = 0;
  RDCASSERT(offset + sz <= totalsize);

  // offset must be aligned
  curoffset = AlignUp(offset + size, align);

  if(bindoffset)
    *bindoffset = (uint32_t)offset;

  void *ptr = NULL;
  VkResult vkr = m_pDriver->vkMapMemory(device, mem, offset, size, 0, (void **)&ptr);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
  return ptr;
}

void *VulkanDebugManager::GPUBuffer::Map(VkDeviceSize &bindoffset, VkDeviceSize usedsize)
{
  uint32_t offs = 0;

  void *ret = Map(&offs, usedsize);

  bindoffset = offs;

  return ret;
}

void VulkanDebugManager::GPUBuffer::Unmap()
{
  m_pDriver->vkUnmapMemory(device, mem);
}

struct VulkanBlobShaderCallbacks
{
  bool Create(uint32_t size, byte *data, vector<uint32_t> **ret) const
  {
    RDCASSERT(ret);

    vector<uint32_t> *blob = new vector<uint32_t>();

    blob->resize(size / sizeof(uint32_t));

    memcpy(&(*blob)[0], data, size);

    *ret = blob;

    return true;
  }

  void Destroy(vector<uint32_t> *blob) const { delete blob; }
  uint32_t GetSize(vector<uint32_t> *blob) const
  {
    return (uint32_t)(blob->size() * sizeof(uint32_t));
  }

  byte *GetData(vector<uint32_t> *blob) const { return (byte *)&(*blob)[0]; }
} ShaderCacheCallbacks;

string VulkanDebugManager::GetSPIRVBlob(const SPIRVCompilationSettings &settings,
                                        const std::vector<std::string> &sources,
                                        vector<uint32_t> **outBlob)
{
  RDCASSERT(sources.size() > 0);

  uint32_t hash = strhash(sources[0].c_str());
  for(size_t i = 1; i < sources.size(); i++)
    hash = strhash(sources[i].c_str(), hash);

  char typestr[3] = {'a', 'a', 0};
  typestr[0] += (char)settings.stage;
  typestr[1] += (char)settings.lang;
  hash = strhash(typestr, hash);

  if(m_ShaderCache.find(hash) != m_ShaderCache.end())
  {
    *outBlob = m_ShaderCache[hash];
    return "";
  }

  vector<uint32_t> *spirv = new vector<uint32_t>();
  string errors = CompileSPIRV(settings, sources, *spirv);

  if(!errors.empty())
  {
    string logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCWARN("Shader compile error:\n%s", logerror.c_str());

    delete spirv;
    *outBlob = NULL;
    return errors;
  }

  *outBlob = spirv;

  if(m_CacheShaders)
  {
    m_ShaderCache[hash] = spirv;
    m_ShaderCacheDirty = true;
  }

  return errors;
}

VulkanDebugManager::VulkanDebugManager(WrappedVulkan *driver, VkDevice dev)
{
  m_pDriver = driver;
  m_State = m_pDriver->GetState();

  driver->GetReplay()->PostDeviceInitCounters();

  m_ResourceManager = m_pDriver->GetResourceManager();

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Zero initialise all of the members so that when deleting we can just destroy everything and let
  // objects that weren't created just silently be skipped

  m_DescriptorPool = VK_NULL_HANDLE;
  m_LinearSampler = VK_NULL_HANDLE;
  m_PointSampler = VK_NULL_HANDLE;

  m_CheckerboardDescSetLayout = VK_NULL_HANDLE;
  m_CheckerboardPipeLayout = VK_NULL_HANDLE;
  m_CheckerboardDescSet = VK_NULL_HANDLE;
  m_CheckerboardPipeline = VK_NULL_HANDLE;
  m_CheckerboardMSAAPipeline = VK_NULL_HANDLE;
  RDCEraseEl(m_CheckerboardUBO);

  m_TexDisplayDescSetLayout = VK_NULL_HANDLE;
  m_TexDisplayPipeLayout = VK_NULL_HANDLE;
  RDCEraseEl(m_TexDisplayDescSet);
  m_TexDisplayNextSet = 0;
  m_TexDisplayPipeline = VK_NULL_HANDLE;
  m_TexDisplayBlendPipeline = VK_NULL_HANDLE;
  m_TexDisplayF32Pipeline = VK_NULL_HANDLE;
  RDCEraseEl(m_TexDisplayUBO);

  RDCEraseEl(m_TexDisplayDummyImages);
  RDCEraseEl(m_TexDisplayDummyImageViews);
  RDCEraseEl(m_TexDisplayDummyWrites);
  RDCEraseEl(m_TexDisplayDummyInfos);
  m_TexDisplayDummyMemory = VK_NULL_HANDLE;

  m_CustomTexWidth = m_CustomTexHeight = 0;
  m_CustomTexImg = VK_NULL_HANDLE;
  RDCEraseEl(m_CustomTexImgView);
  m_CustomTexMemSize = 0;
  m_CustomTexMem = VK_NULL_HANDLE;
  m_CustomTexFB = VK_NULL_HANDLE;
  m_CustomTexRP = VK_NULL_HANDLE;
  m_CustomTexPipeline = VK_NULL_HANDLE;

  m_PickPixelImageMem = VK_NULL_HANDLE;
  m_PickPixelImage = VK_NULL_HANDLE;
  m_PickPixelImageView = VK_NULL_HANDLE;
  m_PickPixelFB = VK_NULL_HANDLE;
  m_PickPixelRP = VK_NULL_HANDLE;

  m_ArrayMSDescSetLayout = VK_NULL_HANDLE;
  m_ArrayMSPipeLayout = VK_NULL_HANDLE;
  m_ArrayMSDescSet = VK_NULL_HANDLE;
  m_Array2MSPipe = VK_NULL_HANDLE;
  m_MS2ArrayPipe = VK_NULL_HANDLE;

  m_TextDescSetLayout = VK_NULL_HANDLE;
  m_TextPipeLayout = VK_NULL_HANDLE;
  m_TextDescSet = VK_NULL_HANDLE;
  RDCEraseEl(m_TextPipeline);
  RDCEraseEl(m_TextGeneralUBO);
  RDCEraseEl(m_TextGlyphUBO);
  RDCEraseEl(m_TextStringUBO);
  m_TextAtlas = VK_NULL_HANDLE;
  m_TextAtlasMem = VK_NULL_HANDLE;
  m_TextAtlasView = VK_NULL_HANDLE;

  m_OverlayImageMem = VK_NULL_HANDLE;
  m_OverlayImage = VK_NULL_HANDLE;
  m_OverlayImageView = VK_NULL_HANDLE;
  m_OverlayNoDepthFB = VK_NULL_HANDLE;
  m_OverlayNoDepthRP = VK_NULL_HANDLE;
  RDCEraseEl(m_OverlayDim);
  m_OverlayMemSize = 0;

  m_QuadDescSetLayout = VK_NULL_HANDLE;
  m_QuadResolvePipeLayout = VK_NULL_HANDLE;
  m_QuadDescSet = VK_NULL_HANDLE;
  RDCEraseEl(m_QuadResolvePipeline);
  m_QuadSPIRV = NULL;

  m_TriSizeDescSetLayout = VK_NULL_HANDLE;
  m_TriSizeDescSet = VK_NULL_HANDLE;
  m_TriSizePipeLayout = VK_NULL_HANDLE;
  m_TriSizeGSModule = VK_NULL_HANDLE;
  m_TriSizeFSModule = VK_NULL_HANDLE;

  m_MeshDescSetLayout = VK_NULL_HANDLE;
  m_MeshPipeLayout = VK_NULL_HANDLE;
  m_MeshDescSet = VK_NULL_HANDLE;
  RDCEraseEl(m_MeshModules);

  m_HistogramDescSetLayout = VK_NULL_HANDLE;
  m_HistogramPipeLayout = VK_NULL_HANDLE;
  RDCEraseEl(m_HistogramDescSet);
  RDCEraseEl(m_MinMaxResultPipe);
  RDCEraseEl(m_MinMaxTilePipe);
  RDCEraseEl(m_HistogramPipe);

  m_OutlineDescSetLayout = VK_NULL_HANDLE;
  m_OutlinePipeLayout = VK_NULL_HANDLE;
  m_OutlineDescSet = VK_NULL_HANDLE;
  RDCEraseEl(m_OutlinePipeline);

  m_MeshFetchDescSetLayout = VK_NULL_HANDLE;
  m_MeshFetchDescSet = VK_NULL_HANDLE;

  m_MeshPickDescSetLayout = VK_NULL_HANDLE;
  m_MeshPickDescSet = VK_NULL_HANDLE;
  m_MeshPickLayout = VK_NULL_HANDLE;
  m_MeshPickPipeline = VK_NULL_HANDLE;

  m_FontCharSize = 1.0f;
  m_FontCharAspect = 1.0f;

  m_FixedColSPIRV = NULL;

  m_Device = dev;

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Do some work that's needed both during capture and during replay

  // Load shader cache, if present
  bool success = LoadShaderCache("vkshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, ShaderCacheCallbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  VkResult vkr = VK_SUCCESS;

  // create linear sampler
  VkSamplerCreateInfo sampInfo = {
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      NULL,
      0,
      VK_FILTER_LINEAR,
      VK_FILTER_LINEAR,
      VK_SAMPLER_MIPMAP_MODE_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      0.0f,     // lod bias
      false,    // enable aniso
      1.0f,     // max aniso
      false,
      VK_COMPARE_OP_NEVER,
      0.0f,
      128.0f,    // min/max lod
      VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      false,    // unnormalized
  };

  vkr = m_pDriver->vkCreateSampler(dev, &sampInfo, NULL, &m_LinearSampler);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkDescriptorPoolSize captureDescPoolTypes[] = {
      {
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
      },
      {
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3,
      },
      {
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3,
      },
      {
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
      },
  };

  VkDescriptorPoolSize replayDescPoolTypes[] = {
      {
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128,
      },
      {
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 128,
      },
      {
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 320,
      },
      {
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32,
      },
      {
          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32,
      },
  };

  VkDescriptorPoolCreateInfo descpoolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      10 + ARRAY_COUNT(m_TexDisplayDescSet),
      ARRAY_COUNT(replayDescPoolTypes),
      &replayDescPoolTypes[0],
  };

  // during capture we only need one text descriptor set, so rather than
  // trying to wait and steal descriptors from a user-side pool, we just
  // create our own very small pool.
  if(m_State >= WRITING)
  {
    descpoolInfo.maxSets = 2;
    descpoolInfo.poolSizeCount = ARRAY_COUNT(captureDescPoolTypes);
    descpoolInfo.pPoolSizes = &captureDescPoolTypes[0];
  }

  // create descriptor pool
  vkr = m_pDriver->vkCreateDescriptorPool(dev, &descpoolInfo, NULL, &m_DescriptorPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // declare some common creation info structs
  VkPipelineLayoutCreateInfo pipeLayoutInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      NULL,
      0,
      1,
      NULL,
      0,
      NULL,    // push constant ranges
  };

  VkDescriptorSetAllocateInfo descSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                  NULL, m_DescriptorPool, 1, NULL};

  // compatible render passes for creating pipelines.
  // Only one of these is needed during capture for the pipeline create, but
  // they are short-lived so just create all of them and share creation code
  VkRenderPass RGBA32RP = VK_NULL_HANDLE;
  VkRenderPass RGBA8sRGBRP = VK_NULL_HANDLE;
  VkRenderPass RGBA16RP = VK_NULL_HANDLE;
  VkRenderPass RGBA8MSRP = VK_NULL_HANDLE;
  VkRenderPass RGBA16MSRP[8] = {0};
  VkRenderPass RGBA8LinearRP = VK_NULL_HANDLE;
  VkRenderPass BGRA8sRGBRP = VK_NULL_HANDLE;
  VkRenderPass BGRA8LinearRP = VK_NULL_HANDLE;

  RDCCOMPILE_ASSERT(ARRAY_COUNT(RGBA16MSRP) == ARRAY_COUNT(m_OutlinePipeline),
                    "Arrays are mismatched in size!");
  RDCCOMPILE_ASSERT(ARRAY_COUNT(RGBA16MSRP) == ARRAY_COUNT(m_QuadResolvePipeline),
                    "Arrays are mismatched in size!");

  {
    VkAttachmentDescription attDesc = {0,
                                       VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_SAMPLE_COUNT_1_BIT,
                                       VK_ATTACHMENT_LOAD_OP_LOAD,
                                       VK_ATTACHMENT_STORE_OP_STORE,
                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                       VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub = {
        0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,    NULL,       // inputs
        1,    &attRef,    // color
        NULL,             // resolve
        NULL,             // depth-stencil
        0,    NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        1,
        &attDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA8sRGBRP);

    attDesc.format = VK_FORMAT_R8G8B8A8_UNORM;

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA8LinearRP);

    attDesc.format = VK_FORMAT_B8G8R8A8_SRGB;

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &BGRA8sRGBRP);

    attDesc.format = VK_FORMAT_B8G8R8A8_UNORM;

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &BGRA8LinearRP);

    attDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA32RP);

    attDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA16RP);

    attDesc.samples = VULKAN_MESH_VIEW_SAMPLES;
    attDesc.format = VK_FORMAT_R8G8B8A8_SRGB;

    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA8MSRP);

    attDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    uint32_t samplesHandled = 0;

    // create a 16F multisampled renderpass for each possible multisample size
    for(size_t i = 0; i < ARRAY_COUNT(RGBA16MSRP); i++)
    {
      attDesc.samples = VkSampleCountFlagBits(1 << i);

      if(m_pDriver->GetDeviceProps().limits.framebufferColorSampleCounts & (uint32_t)attDesc.samples)
      {
        m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA16MSRP[i]);

        samplesHandled |= (uint32_t)attDesc.samples;
      }
    }

    RDCASSERTEQUAL((uint32_t)m_pDriver->GetDeviceProps().limits.framebufferColorSampleCounts,
                   samplesHandled);
  }

  // declare the pipeline creation info and all of its sub-structures
  // these are modified as appropriate for each pipeline we create
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
       VK_NULL_HANDLE, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
       VK_NULL_HANDLE, "main", NULL},
  };

  VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      NULL,
      0,
      0,
      NULL,    // vertex bindings
      0,
      NULL,    // vertex attributes
  };

  VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      NULL,
      0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      false,
  };

  VkRect2D scissor = {{0, 0}, {16384, 16384}};

  VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, &scissor};

  VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      false,
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_NONE,
      VK_FRONT_FACE_CLOCKWISE,
      false,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
  };

  VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      NULL,
      0,
      VK_SAMPLE_COUNT_1_BIT,
      false,
      0.0f,
      NULL,
      false,
      false,
  };

  VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      false,
      VK_COMPARE_OP_ALWAYS,
      false,
      false,
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      0.0f,
      1.0f,
  };

  VkPipelineColorBlendAttachmentState attState = {
      false,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      0xf,
  };

  VkPipelineColorBlendStateCreateInfo cb = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      VK_LOGIC_OP_NO_OP,
      1,
      &attState,
      {1.0f, 1.0f, 1.0f, 1.0f}};

  VkDynamicState dynstates[] = {VK_DYNAMIC_STATE_VIEWPORT};

  VkPipelineDynamicStateCreateInfo dyn = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      NULL,
      0,
      ARRAY_COUNT(dynstates),
      dynstates,
  };

  VkGraphicsPipelineCreateInfo pipeInfo = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      NULL,
      0,
      2,
      stages,
      &vi,
      &ia,
      NULL,    // tess
      &vp,
      &rs,
      &msaa,
      &ds,
      &cb,
      &dyn,
      VK_NULL_HANDLE,
      RGBA8sRGBRP,
      0,                 // sub pass
      VK_NULL_HANDLE,    // base pipeline handle
      -1,                // base pipeline index
  };

  VkComputePipelineCreateInfo compPipeInfo = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      NULL,
      0,
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_COMPUTE_BIT,
       VK_NULL_HANDLE, "main", NULL},
      VK_NULL_HANDLE,
      VK_NULL_HANDLE,
      0,    // base pipeline VkPipeline
  };

  // declare a few more misc things that are needed on both paths
  VkDescriptorBufferInfo bufInfo[8];
  RDCEraseEl(bufInfo);

  vector<string> sources;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  // A workaround for a couple of bugs, removing texelFetch use from shaders.
  // It means broken functionality but at least no instant crashes
  bool texelFetchBrokenDriver = false;

  VkDriverInfo driverVersion = m_pDriver->GetDriverVersion();

  if(driverVersion.IsNV())
  {
    // drivers before 372.54 did not handle a glslang bugfix about separated samplers,
    // and disabling texelFetch works as a workaround.

    if(driverVersion.Major() < 372 || (driverVersion.Major() == 372 && driverVersion.Minor() < 54))
      texelFetchBrokenDriver = true;
  }

// only check this on windows. This is a bit of a hack, as really we want to check if we're
// using the AMD official driver, but there's not a great other way to distinguish it from
// the RADV open source driver.
#if ENABLED(RDOC_WIN32)
  if(driverVersion.IsAMD())
  {
    // for AMD the bugfix version isn't clear as version numbering wasn't strong for a while, but
    // any driver that reports a version of >= 1.0.0 is fine, as previous versions all reported
    // 0.9.0 as the version.

    if(driverVersion.Major() < 1)
      texelFetchBrokenDriver = true;
  }
#endif

  if(texelFetchBrokenDriver)
  {
    RDCWARN(
        "Detected an older driver, enabling texelFetch workaround - try updating to the latest "
        "version");
  }

  // another workaround, on some AMD driver versions creating an MSAA image with STORAGE_BIT
  // causes graphical corruption trying to sample from it. We workaround it by preventing the
  // MSAA <-> Array pipelines from creating, which removes the STORAGE_BIT and skips the copies.
  // It means initial contents of MSAA images are missing but that's less important than being
  // able to inspect MSAA images properly.
  bool amdStorageMSAABrokenDriver = false;

// same as above, only affects the AMD official driver
#if ENABLED(RDOC_WIN32)
  if(driverVersion.IsAMD())
  {
    // not fixed yet
    amdStorageMSAABrokenDriver = true;
  }
#endif

  SPIRVCompilationSettings compileSettings;
  compileSettings.lang = SPIRVSourceLanguage::VulkanGLSL;

  // needed in both replay and capture, create depth MS->array pipelines
  {
    {
      VkDescriptorSetLayoutBinding layoutBinding[] = {
          {
              0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
          },
          {
              1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
          },
          {
              2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL,
          },
      };

      VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          NULL,
          0,
          ARRAY_COUNT(layoutBinding),
          &layoutBinding[0],
      };

      vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                   &m_ArrayMSDescSetLayout);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    pipeLayoutInfo.pSetLayouts = &m_ArrayMSDescSetLayout;

    VkPushConstantRange push = {VK_SHADER_STAGE_ALL, 0, sizeof(Vec4u)};

    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &push;

    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_ArrayMSPipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    pipeLayoutInfo.pushConstantRangeCount = 0;
    pipeLayoutInfo.pPushConstantRanges = NULL;

    descSetAllocInfo.pSetLayouts = &m_ArrayMSDescSetLayout;
    vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_ArrayMSDescSet);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    enum
    {
      VS,
      MS2ARR,
      ARR2MS
    };

    std::string srcs[] = {
        GetEmbeddedResource(glsl_blit_vert), GetEmbeddedResource(glsl_depthms2arr_frag),
        GetEmbeddedResource(glsl_deptharr2ms_frag),
    };

    VkShaderModule modules[3];

    for(size_t i = 0; i < ARRAY_COUNT(srcs); i++)
    {
      GenerateGLSLShader(sources, eShaderVulkan, "", srcs[i], 430);

      vector<uint32_t> *spirv;

      compileSettings.stage = i == 0 ? SPIRVShaderStage::Vertex : SPIRVShaderStage::Fragment;
      string err = GetSPIRVBlob(compileSettings, sources, &spirv);
      RDCASSERT(err.empty() && spirv);

      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          NULL,
          0,
          spirv->size() * sizeof(uint32_t),
          &(*spirv)[0],
      };

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &modules[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    stages[0].module = modules[VS];
    stages[1].module = modules[MS2ARR];

    VkFormat formats[] = {
        VK_FORMAT_D16_UNORM,         VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32,
        VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT,        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    VkSampleCountFlagBits sampleCounts[] = {
        VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT,
    };

    // we use VK_IMAGE_LAYOUT_GENERAL here because it matches the expected layout for the
    // non-depth copy, which uses a storage image.
    VkAttachmentDescription attDesc = {0,
                                       VK_FORMAT_UNDEFINED,
                                       VK_SAMPLE_COUNT_1_BIT,
                                       VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_ATTACHMENT_STORE_OP_STORE,
                                       VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_ATTACHMENT_STORE_OP_STORE,
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_GENERAL};

    VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_GENERAL};

    VkSubpassDescription sub = {
        0,       VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,       NULL,    // inputs
        0,       NULL,    // color
        NULL,             // resolve
        &attRef,          // depth-stencil
        0,       NULL,    // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        1,
        &attDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    VkDynamicState depthcopy_dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_STENCIL_REFERENCE};

    VkPipelineDepthStencilStateCreateInfo depthcopy_ds = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        NULL,
        0,
        true,
        true,
        VK_COMPARE_OP_ALWAYS,
        false,
        true,
        {VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS,
         0xff, 0xff, 0},
        {VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS,
         0xff, 0xff, 0},
        0.0f,
        1.0f,
    };

    pipeInfo.layout = m_ArrayMSPipeLayout;
    dyn.dynamicStateCount = ARRAY_COUNT(depthcopy_dyn);
    dyn.pDynamicStates = depthcopy_dyn;
    pipeInfo.pDepthStencilState = &depthcopy_ds;

    cb.attachmentCount = 0;

    RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthMS2ArrayPipe) == ARRAY_COUNT(formats),
                      "Array count mismatch");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthArray2MSPipe) == ARRAY_COUNT(formats),
                      "Array count mismatch");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthArray2MSPipe[0]) == ARRAY_COUNT(sampleCounts),
                      "Array count mismatch");

    for(size_t f = 0; f < ARRAY_COUNT(formats); f++)
    {
      attDesc.format = formats[f];
      stages[1].module = modules[MS2ARR];

      VkRenderPass rp;

      vkr = m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &rp);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      pipeInfo.renderPass = rp;

      vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                                 &m_DepthMS2ArrayPipe[f]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->vkDestroyRenderPass(dev, rp, NULL);

      stages[1].module = modules[ARR2MS];

      for(size_t s = 0; s < ARRAY_COUNT(sampleCounts); s++)
      {
        attDesc.samples = sampleCounts[s];
        msaa.rasterizationSamples = sampleCounts[s];
        msaa.sampleShadingEnable = true;
        msaa.minSampleShading = 1.0f;

        vkr = m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &rp);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        pipeInfo.renderPass = rp;

        vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                                   &m_DepthArray2MSPipe[f][s]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        m_pDriver->vkDestroyRenderPass(dev, rp, NULL);

        attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        msaa.sampleShadingEnable = false;
        msaa.minSampleShading = 0.0f;
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      }
    }

    // restore pipeline state to normal
    cb.attachmentCount = 1;

    pipeInfo.renderPass = RGBA8sRGBRP;
    dyn.dynamicStateCount = ARRAY_COUNT(dynstates);
    dyn.pDynamicStates = dynstates;
    pipeInfo.pDepthStencilState = &ds;

    for(size_t i = 0; i < ARRAY_COUNT(modules); i++)
      m_pDriver->vkDestroyShaderModule(dev, modules[i], NULL);
  }

  //////////////////////////////////////////////////////////////////////////////////////
  // if we're writing, only create text-rendering related resources,
  // then tidy up early and return
  if(m_State >= WRITING)
  {
    {
      VkDescriptorSetLayoutBinding layoutBinding[] = {
          {
              0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL,
          },
          {
              1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
          },
          {
              2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL,
          },
          {
              3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
          }};

      VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          NULL,
          0,
          ARRAY_COUNT(layoutBinding),
          &layoutBinding[0],
      };

      vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                   &m_TextDescSetLayout);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    pipeLayoutInfo.pSetLayouts = &m_TextDescSetLayout;

    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_TextPipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    descSetAllocInfo.pSetLayouts = &m_TextDescSetLayout;
    vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_TextDescSet);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_TextGeneralUBO.Create(
        driver, dev, 128, 100,
        0);    // make the ring conservatively large to handle many lines of text * several frames
    RDCCOMPILE_ASSERT(sizeof(FontUBOData) <= 128, "font uniforms size");

    m_TextStringUBO.Create(driver, dev, 4096, 10, 0);    // we only use a subset of the
                                                         // [MAX_SINGLE_LINE_LENGTH] array needed
                                                         // for each line, so this ring can be
                                                         // smaller
    RDCCOMPILE_ASSERT(sizeof(StringUBOData) <= 4096, "font uniforms size");

    attState.blendEnable = true;
    attState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    VkShaderModule ms2arrayModule = VK_NULL_HANDLE, array2msModule = VK_NULL_HANDLE;

    for(size_t i = 0; i < 2; i++)
    {
      GenerateGLSLShader(
          sources, eShaderVulkan, "",
          i == 0 ? GetEmbeddedResource(glsl_text_vert) : GetEmbeddedResource(glsl_text_frag), 430);

      vector<uint32_t> *spirv;

      compileSettings.stage = i == 0 ? SPIRVShaderStage::Vertex : SPIRVShaderStage::Fragment;
      string err = GetSPIRVBlob(compileSettings, sources, &spirv);
      RDCASSERT(err.empty() && spirv);

      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          NULL,
          0,
          spirv->size() * sizeof(uint32_t),
          &(*spirv)[0],
      };

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &stages[i].module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    for(size_t i = 0; i < 2; i++)
    {
      GenerateGLSLShader(sources, eShaderVulkan, "", i == 0 ? GetEmbeddedResource(glsl_array2ms_comp)
                                                            : GetEmbeddedResource(glsl_ms2array_comp),
                         430, false);

      vector<uint32_t> *spirv;

      compileSettings.stage = SPIRVShaderStage::Compute;
      string err = GetSPIRVBlob(compileSettings, sources, &spirv);
      RDCASSERT(err.empty() && spirv);

      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          NULL,
          0,
          spirv->size() * sizeof(uint32_t),
          &(*spirv)[0],
      };

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL,
                                            i == 0 ? &array2msModule : &ms2arrayModule);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    if(!texelFetchBrokenDriver && !amdStorageMSAABrokenDriver &&
       m_pDriver->GetDeviceFeatures().shaderStorageImageMultisample &&
       m_pDriver->GetDeviceFeatures().shaderStorageImageWriteWithoutFormat)
    {
      compPipeInfo.stage.module = ms2arrayModule;
      compPipeInfo.layout = m_ArrayMSPipeLayout;

      vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                &m_MS2ArrayPipe);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      compPipeInfo.stage.module = array2msModule;
      compPipeInfo.layout = m_ArrayMSPipeLayout;

      vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                &m_Array2MSPipe);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    pipeInfo.layout = m_TextPipeLayout;

    vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                               &m_TextPipeline[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    pipeInfo.renderPass = RGBA8LinearRP;

    vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                               &m_TextPipeline[1]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    pipeInfo.renderPass = BGRA8sRGBRP;

    vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                               &m_TextPipeline[2]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    pipeInfo.renderPass = BGRA8LinearRP;

    vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                               &m_TextPipeline[3]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkDestroyShaderModule(dev, array2msModule, NULL);
    m_pDriver->vkDestroyShaderModule(dev, ms2arrayModule, NULL);
    m_pDriver->vkDestroyShaderModule(dev, stages[0].module, NULL);
    m_pDriver->vkDestroyShaderModule(dev, stages[1].module, NULL);

    // create the actual font texture data and glyph data, for upload
    {
      const uint32_t width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

      VkImageCreateInfo imInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          NULL,
          0,
          VK_IMAGE_TYPE_2D,
          VK_FORMAT_R8_UNORM,
          {width, height, 1},
          1,
          1,
          VK_SAMPLE_COUNT_1_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          VK_SHARING_MODE_EXCLUSIVE,
          0,
          NULL,
          VK_IMAGE_LAYOUT_UNDEFINED,
      };

      string font = GetEmbeddedResource(sourcecodepro_ttf);
      byte *ttfdata = (byte *)font.c_str();

      const int firstChar = FONT_FIRST_CHAR;
      const int lastChar = FONT_LAST_CHAR;
      const int numChars = lastChar - firstChar + 1;

      RDCCOMPILE_ASSERT(FONT_FIRST_CHAR == int(' '), "Font defines are messed up");

      byte *buf = new byte[width * height];

      const float pixelHeight = 20.0f;

      stbtt_bakedchar chardata[numChars];
      stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars,
                           chardata);

      m_FontCharSize = pixelHeight;
      m_FontCharAspect = chardata->xadvance / pixelHeight;

      stbtt_fontinfo f = {0};
      stbtt_InitFont(&f, ttfdata, 0);

      int ascent = 0;
      stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

      float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, pixelHeight);

      // create and fill image
      {
        vkr = m_pDriver->vkCreateImage(dev, &imInfo, NULL, &m_TextAtlas);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkMemoryRequirements mrq = {0};
        m_pDriver->vkGetImageMemoryRequirements(dev, m_TextAtlas, &mrq);

        // allocate readback memory
        VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
            driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
        };

        vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &m_TextAtlasMem);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        vkr = m_pDriver->vkBindImageMemory(dev, m_TextAtlas, m_TextAtlasMem, 0);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageViewCreateInfo viewInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            NULL,
            0,
            m_TextAtlas,
            VK_IMAGE_VIEW_TYPE_2D,
            imInfo.format,
            {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
             VK_COMPONENT_SWIZZLE_ONE},
            {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
            },
        };

        vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &m_TextAtlasView);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        // create temporary memory and buffer to upload atlas
        m_TextAtlasUpload.Create(driver, dev, 32768, 1,
                                 0);    // doesn't need to be ring'd, as it's static
        RDCCOMPILE_ASSERT(width * height <= 32768, "font uniform size");

        byte *pData = (byte *)m_TextAtlasUpload.Map();
        RDCASSERT(pData);

        memcpy(pData, buf, width * height);

        m_TextAtlasUpload.Unmap();
      }

      m_TextGlyphUBO.Create(driver, dev, 4096, 1,
                            0);    // doesn't need to be ring'd, as it's static
      RDCCOMPILE_ASSERT(sizeof(Vec4f) * 2 * (numChars + 1) < 4096, "font uniform size");

      FontGlyphData *glyphData = (FontGlyphData *)m_TextGlyphUBO.Map();

      glyphData[0].posdata = Vec4f();
      glyphData[0].uvdata = Vec4f();

      for(int i = 1; i < numChars; i++)
      {
        stbtt_bakedchar *b = chardata + i;

        float x = b->xoff;
        float y = b->yoff + maxheight;

        glyphData[i].posdata =
            Vec4f(x / b->xadvance, y / pixelHeight, b->xadvance / float(b->x1 - b->x0),
                  pixelHeight / float(b->y1 - b->y0));
        glyphData[i].uvdata = Vec4f(b->x0, b->y0, b->x1, b->y1);
      }

      m_TextGlyphUBO.Unmap();
    }

    // perform GPU copy from m_TextAtlasUpload to m_TextAtlas with appropriate barriers
    {
      VkCommandBuffer textAtlasUploadCmd = driver->GetNextCmd();

      vkr = ObjDisp(textAtlasUploadCmd)->BeginCommandBuffer(Unwrap(textAtlasUploadCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      // need to update image layout into valid state first
      VkImageMemoryBarrier copysrcbarrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          0,
          VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          0,
          0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
          Unwrap(m_TextAtlas),
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      DoPipelineBarrier(textAtlasUploadCmd, 1, &copysrcbarrier);

      VkBufferMemoryBarrier uploadbarrier = {
          VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_HOST_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(m_TextAtlasUpload.buf),
          0,
          m_TextAtlasUpload.totalsize,
      };

      // ensure host writes finish before copy
      DoPipelineBarrier(textAtlasUploadCmd, 1, &uploadbarrier);

      VkBufferImageCopy bufRegion = {
          0,
          0,
          0,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          {
              0, 0, 0,
          },
          {FONT_TEX_WIDTH, FONT_TEX_HEIGHT, 1},
      };

      // copy to image
      ObjDisp(textAtlasUploadCmd)
          ->CmdCopyBufferToImage(Unwrap(textAtlasUploadCmd), Unwrap(m_TextAtlasUpload.buf),
                                 Unwrap(m_TextAtlas), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                 &bufRegion);

      VkImageMemoryBarrier copydonebarrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          copysrcbarrier.dstAccessMask,
          VK_ACCESS_SHADER_READ_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          0,
          0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
          Unwrap(m_TextAtlas),
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      // ensure atlas is filled before reading in shader
      DoPipelineBarrier(textAtlasUploadCmd, 1, &copydonebarrier);

      ObjDisp(textAtlasUploadCmd)->EndCommandBuffer(Unwrap(textAtlasUploadCmd));
    }

    m_TextGeneralUBO.FillDescriptor(bufInfo[0]);
    m_TextGlyphUBO.FillDescriptor(bufInfo[1]);
    m_TextStringUBO.FillDescriptor(bufInfo[2]);

    VkDescriptorImageInfo atlasImInfo;
    atlasImInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    atlasImInfo.imageView = Unwrap(m_TextAtlasView);
    atlasImInfo.sampler = Unwrap(m_LinearSampler);

    VkWriteDescriptorSet textSetWrites[] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_TextDescSet), 0, 0, 1,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &bufInfo[0], NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_TextDescSet), 1, 0, 1,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufInfo[1], NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_TextDescSet), 2, 0, 1,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &bufInfo[2], NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_TextDescSet), 3, 0, 1,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &atlasImInfo, NULL, NULL},
    };

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(textSetWrites), textSetWrites, 0,
                                       NULL);

    m_pDriver->vkDestroyRenderPass(dev, RGBA16RP, NULL);
    m_pDriver->vkDestroyRenderPass(dev, RGBA32RP, NULL);
    m_pDriver->vkDestroyRenderPass(dev, RGBA8sRGBRP, NULL);
    m_pDriver->vkDestroyRenderPass(dev, RGBA8MSRP, NULL);
    for(size_t i = 0; i < ARRAY_COUNT(RGBA16MSRP); i++)
      m_pDriver->vkDestroyRenderPass(dev, RGBA16MSRP[i], NULL);
    m_pDriver->vkDestroyRenderPass(dev, RGBA8LinearRP, NULL);
    m_pDriver->vkDestroyRenderPass(dev, BGRA8sRGBRP, NULL);
    m_pDriver->vkDestroyRenderPass(dev, BGRA8LinearRP, NULL);

    return;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  // everything created below this point is only needed during replay, and will be NULL
  // while in the captured application

  // create point sampler
  sampInfo.minFilter = VK_FILTER_NEAREST;
  sampInfo.magFilter = VK_FILTER_NEAREST;

  vkr = m_pDriver->vkCreateSampler(dev, &sampInfo, NULL, &m_PointSampler);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {{
        0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL,
    }};

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_CheckerboardDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // identical layout
    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL, &m_MeshDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // identical layout
    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_OutlineDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {{
        0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
    }};

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_MeshFetchDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
    };

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_MeshPickDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            15, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
    };

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_TexDisplayDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {
            0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
    };

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL, &m_QuadDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL,
        },
    };

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_TriSizeDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {
            0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
        {
            19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL,
        },
    };

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(layoutBinding),
        &layoutBinding[0],
    };

    vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL,
                                                 &m_HistogramDescSetLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  pipeLayoutInfo.pSetLayouts = &m_TexDisplayDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_TexDisplayPipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_CheckerboardDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_CheckerboardPipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_QuadDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_QuadResolvePipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_TriSizeDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_TriSizePipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_OutlineDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_OutlinePipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_MeshDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_MeshPipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_HistogramDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_HistogramPipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeLayoutInfo.pSetLayouts = &m_MeshPickDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_MeshPickLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_CheckerboardDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_CheckerboardDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_TexDisplayDescSetLayout;
  for(size_t i = 0; i < ARRAY_COUNT(m_TexDisplayDescSet); i++)
  {
    vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_TexDisplayDescSet[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  descSetAllocInfo.pSetLayouts = &m_QuadDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_QuadDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_TriSizeDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_TriSizeDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_OutlineDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_OutlineDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_MeshDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_MeshDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_HistogramDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_HistogramDescSet[0]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_HistogramDescSet[1]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_MeshFetchDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_MeshFetchDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  descSetAllocInfo.pSetLayouts = &m_MeshPickDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_MeshPickDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // sizes are always 0 so that these buffers are created on demand
  m_MeshPickIBSize = 0;
  m_MeshPickVBSize = 0;

  m_MeshPickUBO.Create(driver, dev, 128, 1, 0);
  RDCCOMPILE_ASSERT(sizeof(MeshPickUBOData) <= 128, "mesh pick UBO size");

  const size_t meshPickResultSize = maxMeshPicks * sizeof(FloatVector) + sizeof(uint32_t);

  m_MeshPickResult.Create(driver, dev, meshPickResultSize, 1,
                          GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
  m_MeshPickResultReadback.Create(driver, dev, meshPickResultSize, 1, GPUBuffer::eGPUBufferReadback);

  m_ReadbackWindow.Create(driver, dev, STAGE_BUFFER_BYTE_SIZE, 1, GPUBuffer::eGPUBufferReadback);

  m_OutlineUBO.Create(driver, dev, 128, 10, 0);
  RDCCOMPILE_ASSERT(sizeof(OutlineUBOData) <= 128, "outline UBO size");

  m_CheckerboardUBO.Create(driver, dev, 128, 10, 0);
  m_TexDisplayUBO.Create(driver, dev, 128, 10, 0);

  RDCCOMPILE_ASSERT(sizeof(TexDisplayUBOData) <= 128, "tex display size");

  string shaderSources[] = {
      GetEmbeddedResource(glsl_blit_vert),        GetEmbeddedResource(glsl_checkerboard_frag),
      GetEmbeddedResource(glsl_texdisplay_frag),  GetEmbeddedResource(glsl_mesh_vert),
      GetEmbeddedResource(glsl_mesh_geom),        GetEmbeddedResource(glsl_mesh_frag),
      GetEmbeddedResource(glsl_minmaxtile_comp),  GetEmbeddedResource(glsl_minmaxresult_comp),
      GetEmbeddedResource(glsl_histogram_comp),   GetEmbeddedResource(glsl_outline_frag),
      GetEmbeddedResource(glsl_quadresolve_frag), GetEmbeddedResource(glsl_quadwrite_frag),
      GetEmbeddedResource(glsl_mesh_comp),        GetEmbeddedResource(glsl_ms2array_comp),
      GetEmbeddedResource(glsl_array2ms_comp),    GetEmbeddedResource(glsl_trisize_geom),
      GetEmbeddedResource(glsl_trisize_frag),
  };

  SPIRVShaderStage shaderStages[] = {
      SPIRVShaderStage::Vertex,   SPIRVShaderStage::Fragment, SPIRVShaderStage::Fragment,
      SPIRVShaderStage::Vertex,   SPIRVShaderStage::Geometry, SPIRVShaderStage::Fragment,
      SPIRVShaderStage::Compute,  SPIRVShaderStage::Compute,  SPIRVShaderStage::Compute,
      SPIRVShaderStage::Fragment, SPIRVShaderStage::Fragment, SPIRVShaderStage::Fragment,
      SPIRVShaderStage::Compute,  SPIRVShaderStage::Compute,  SPIRVShaderStage::Compute,
      SPIRVShaderStage::Geometry, SPIRVShaderStage::Fragment,
  };

  enum shaderIdx
  {
    BLITVS,
    CHECKERBOARDFS,
    TEXDISPLAYFS,
    MESHVS,
    MESHGS,
    MESHFS,
    MINMAXTILECS,
    MINMAXRESULTCS,
    HISTOGRAMCS,
    OUTLINEFS,
    QUADRESOLVEFS,
    QUADWRITEFS,
    MESHCS,
    MS2ARRAYCS,
    ARRAY2MSCS,
    TRISIZEGS,
    TRISIZEFS,
    NUM_SHADERS,
  };

  vector<uint32_t> *shaderSPIRV[NUM_SHADERS];
  VkShaderModule module[NUM_SHADERS];

  RDCCOMPILE_ASSERT(ARRAY_COUNT(shaderSources) == ARRAY_COUNT(shaderStages), "Mismatched arrays!");
  RDCCOMPILE_ASSERT(ARRAY_COUNT(shaderSources) == NUM_SHADERS, "Mismatched arrays!");

  m_CacheShaders = true;

  {
    GenerateGLSLShader(sources, eShaderVulkan, "", GetEmbeddedResource(glsl_fixedcol_frag), 430,
                       false);

    compileSettings.stage = SPIRVShaderStage::Fragment;
    string err = GetSPIRVBlob(compileSettings, sources, &m_FixedColSPIRV);
    RDCASSERT(err.empty() && m_FixedColSPIRV);
  }

  for(size_t i = 0; i < ARRAY_COUNT(module); i++)
  {
    // these modules will be compiled later
    if(i == HISTOGRAMCS || i == MINMAXTILECS || i == MINMAXRESULTCS)
      continue;

    string defines = "";
    if(texelFetchBrokenDriver)
      defines += "#define NO_TEXEL_FETCH\n";

    GenerateGLSLShader(sources, eShaderVulkan, defines, shaderSources[i], 430, i != QUADWRITEFS);

    compileSettings.stage = shaderStages[i];
    string err = GetSPIRVBlob(compileSettings, sources, &shaderSPIRV[i]);
    RDCASSERT(err.empty() && shaderSPIRV[i]);

    VkShaderModuleCreateInfo modinfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        NULL,
        0,
        shaderSPIRV[i]->size() * sizeof(uint32_t),
        &(*shaderSPIRV[i])[0],
    };

    if(i == QUADWRITEFS)
    {
      m_QuadSPIRV = shaderSPIRV[i];
      module[i] = VK_NULL_HANDLE;
      continue;
    }

    vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &module[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  attState.blendEnable = false;

  pipeInfo.layout = m_CheckerboardPipeLayout;
  pipeInfo.renderPass = RGBA8sRGBRP;

  stages[0].module = module[BLITVS];
  stages[1].module = module[CHECKERBOARDFS];

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_CheckerboardPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  msaa.rasterizationSamples = VULKAN_MESH_VIEW_SAMPLES;
  pipeInfo.renderPass = RGBA8MSRP;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_CheckerboardMSAAPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  pipeInfo.renderPass = RGBA8sRGBRP;

  stages[0].module = module[BLITVS];
  stages[1].module = module[TEXDISPLAYFS];

  pipeInfo.layout = m_TexDisplayPipeLayout;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeInfo.renderPass = RGBA32RP;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayF32Pipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeInfo.renderPass = RGBA8sRGBRP;

  attState.blendEnable = true;
  attState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  attState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayBlendPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  stages[0].module = module[BLITVS];
  stages[1].module = module[OUTLINEFS];

  pipeInfo.layout = m_OutlinePipeLayout;

  attState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  attState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

  for(size_t i = 0; i < ARRAY_COUNT(m_OutlinePipeline); i++)
  {
    if(RGBA16MSRP[i] == VK_NULL_HANDLE)
      continue;

    // if we have a 16F renderpass for this sample count then create a pipeline
    pipeInfo.renderPass = RGBA16MSRP[i];

    msaa.rasterizationSamples = VkSampleCountFlagBits(1 << i);

    vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                               &m_OutlinePipeline[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  attState.blendEnable = false;

  stages[0].module = module[BLITVS];
  stages[1].module = module[QUADRESOLVEFS];

  pipeInfo.layout = m_QuadResolvePipeLayout;

  for(size_t i = 0; i < ARRAY_COUNT(m_QuadResolvePipeline); i++)
  {
    if(RGBA16MSRP[i] == VK_NULL_HANDLE)
      continue;

    pipeInfo.renderPass = RGBA16MSRP[i];

    msaa.rasterizationSamples = VkSampleCountFlagBits(1 << i);

    vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                               &m_QuadResolvePipeline[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  compPipeInfo.layout = m_HistogramPipeLayout;

  for(size_t t = eTexType_1D; t < eTexType_Max; t++)
  {
    for(size_t f = 0; f < 3; f++)
    {
      VkShaderModule minmaxtile = VK_NULL_HANDLE;
      VkShaderModule minmaxresult = VK_NULL_HANDLE;
      VkShaderModule histogram = VK_NULL_HANDLE;
      string err;
      vector<uint32_t> *blob = NULL;
      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, 0, NULL,
      };

      string defines = "";
      if(texelFetchBrokenDriver)
        defines += "#define NO_TEXEL_FETCH\n";
      defines += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
      defines += string("#define UINT_TEX ") + (f == 1 ? "1" : "0") + "\n";
      defines += string("#define SINT_TEX ") + (f == 2 ? "1" : "0") + "\n";

      GenerateGLSLShader(sources, eShaderVulkan, defines, shaderSources[HISTOGRAMCS], 430);

      compileSettings.stage = SPIRVShaderStage::Compute;
      err = GetSPIRVBlob(compileSettings, sources, &blob);
      RDCASSERT(err.empty() && blob);

      modinfo.codeSize = blob->size() * sizeof(uint32_t);
      modinfo.pCode = &(*blob)[0];

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &histogram);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GenerateGLSLShader(sources, eShaderVulkan, defines, shaderSources[MINMAXTILECS], 430);

      err = GetSPIRVBlob(compileSettings, sources, &blob);
      RDCASSERT(err.empty() && blob);

      modinfo.codeSize = blob->size() * sizeof(uint32_t);
      modinfo.pCode = &(*blob)[0];

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &minmaxtile);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(t == 1)
      {
        GenerateGLSLShader(sources, eShaderVulkan, defines, shaderSources[MINMAXRESULTCS], 430);

        err = GetSPIRVBlob(compileSettings, sources, &blob);
        RDCASSERT(err.empty() && blob);

        modinfo.codeSize = blob->size() * sizeof(uint32_t);
        modinfo.pCode = &(*blob)[0];

        vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &minmaxresult);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      compPipeInfo.stage.module = minmaxtile;

      vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                &m_MinMaxTilePipe[t][f]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      compPipeInfo.stage.module = histogram;

      vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                &m_HistogramPipe[t][f]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(t == 1)
      {
        compPipeInfo.stage.module = minmaxresult;

        vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                  &m_MinMaxResultPipe[f]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      m_pDriver->vkDestroyShaderModule(dev, histogram, NULL);
      m_pDriver->vkDestroyShaderModule(dev, minmaxtile, NULL);
      if(t == 1)
        m_pDriver->vkDestroyShaderModule(dev, minmaxresult, NULL);
    }
  }

  {
    compPipeInfo.stage.module = module[MESHCS];
    compPipeInfo.layout = m_MeshPickLayout;

    vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                              &m_MeshPickPipeline);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  if(!texelFetchBrokenDriver && !amdStorageMSAABrokenDriver &&
     m_pDriver->GetDeviceFeatures().shaderStorageImageMultisample &&
     m_pDriver->GetDeviceFeatures().shaderStorageImageWriteWithoutFormat)
  {
    compPipeInfo.stage.module = module[MS2ARRAYCS];
    compPipeInfo.layout = m_ArrayMSPipeLayout;

    vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                              &m_MS2ArrayPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    compPipeInfo.stage.module = module[ARRAY2MSCS];
    compPipeInfo.layout = m_ArrayMSPipeLayout;

    vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                              &m_Array2MSPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  m_CacheShaders = false;

  m_pDriver->vkDestroyRenderPass(dev, RGBA16RP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA32RP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8sRGBRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8MSRP, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(RGBA16MSRP); i++)
    m_pDriver->vkDestroyRenderPass(dev, RGBA16MSRP[i], NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8LinearRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, BGRA8sRGBRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, BGRA8LinearRP, NULL);

  for(size_t i = 0; i < ARRAY_COUNT(module); i++)
  {
    // hold onto the shaders/modules we use later
    if(i == MESHVS)
    {
      m_MeshModules[0] = module[i];
    }
    else if(i == MESHGS)
    {
      m_MeshModules[1] = module[i];
    }
    else if(i == MESHFS)
    {
      m_MeshModules[2] = module[i];
    }
    else if(i == TRISIZEGS)
    {
      m_TriSizeGSModule = module[i];
    }
    else if(i == TRISIZEFS)
    {
      m_TriSizeFSModule = module[i];
    }
    else if(i == BLITVS)
    {
      m_BlitVSModule = module[i];
    }
    else if(i == HISTOGRAMCS || i == MINMAXTILECS || i == MINMAXRESULTCS)
    {
      // not compiled normally
      continue;
    }
    else if(module[i] != VK_NULL_HANDLE)
    {
      m_pDriver->vkDestroyShaderModule(dev, module[i], NULL);
    }
  }

  VkCommandBuffer replayDataCmd = driver->GetNextCmd();

  vkr = ObjDisp(replayDataCmd)->BeginCommandBuffer(Unwrap(replayDataCmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // create dummy images for filling out the texdisplay descriptors
  // in slots that are skipped by dynamic branching (e.g. 3D texture
  // when we're displaying a 2D, etc).
  {
    int index = 0;

    VkDeviceSize offsets[ARRAY_COUNT(m_TexDisplayDummyImages)];
    VkDeviceSize curOffset = 0;

    // we pick RGBA8 formats to be guaranteed they will be supported
    VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_SINT};
    VkImageType types[] = {VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_2D};
    VkImageViewType viewtypes[] = {VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                                   VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_VIEW_TYPE_2D};
    VkSampleCountFlagBits sampleCounts[] = {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_1_BIT,
                                            VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT};

    // type max is one higher than the last RESTYPE, and RESTYPES are 1-indexed
    RDCCOMPILE_ASSERT(RESTYPE_TEXTYPEMAX - 1 == ARRAY_COUNT(types),
                      "RESTYPE values don't match formats for dummy images");

    RDCCOMPILE_ASSERT(
        ARRAY_COUNT(m_TexDisplayDummyImages) == ARRAY_COUNT(m_TexDisplayDummyImageViews),
        "dummy image arrays mismatched sizes");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(m_TexDisplayDummyImages) == ARRAY_COUNT(m_TexDisplayDummyWrites),
                      "dummy image arrays mismatched sizes");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(m_TexDisplayDummyImages) == ARRAY_COUNT(m_TexDisplayDummyInfos),
                      "dummy image arrays mismatched sizes");

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, 0, ~0U,
    };

    for(size_t fmt = 0; fmt < ARRAY_COUNT(formats); fmt++)
    {
      for(size_t type = 0; type < ARRAY_COUNT(types); type++)
      {
        // create 1x1 image of the right size
        VkImageCreateInfo imInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            NULL,
            0,
            types[type],
            formats[fmt],
            {1, 1, 1},
            1,
            1,
            sampleCounts[type],
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            NULL,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        vkr = m_pDriver->vkCreateImage(dev, &imInfo, NULL, &m_TexDisplayDummyImages[index]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkMemoryRequirements mrq = {0};
        m_pDriver->vkGetImageMemoryRequirements(dev, m_TexDisplayDummyImages[index], &mrq);

        uint32_t memIndex = driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits);

        // make sure all images can use the same memory type
        RDCASSERTMSG("memory type indices don't overlap!",
                     allocInfo.memoryTypeIndex == ~0U || allocInfo.memoryTypeIndex == memIndex,
                     allocInfo.memoryTypeIndex, memIndex, fmt, type);

        allocInfo.memoryTypeIndex = memIndex;

        // align to our alignment, then increment curOffset by our size
        curOffset = AlignUp(curOffset, mrq.alignment);
        offsets[index] = curOffset;
        curOffset += mrq.size;

        // fill out the descriptor set write to the write binding - set will be filled out
        // on demand when we're actulaly using these writes.
        m_TexDisplayDummyWrites[index].descriptorCount = 1;
        m_TexDisplayDummyWrites[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        m_TexDisplayDummyWrites[index].pNext = NULL;
        m_TexDisplayDummyWrites[index].dstSet = VK_NULL_HANDLE;
        m_TexDisplayDummyWrites[index].dstBinding =
            5 * uint32_t(fmt + 1) + uint32_t(type) + 1;    // 5 + RESTYPE_x
        m_TexDisplayDummyWrites[index].dstArrayElement = 0;
        m_TexDisplayDummyWrites[index].descriptorCount = 1;
        m_TexDisplayDummyWrites[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        m_TexDisplayDummyWrites[index].pImageInfo = &m_TexDisplayDummyInfos[index];
        m_TexDisplayDummyWrites[index].pBufferInfo = NULL;
        m_TexDisplayDummyWrites[index].pTexelBufferView = NULL;

        m_TexDisplayDummyInfos[index].sampler = Unwrap(m_PointSampler);
        m_TexDisplayDummyInfos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        index++;
      }
    }

    // align up a bit just to be safe
    allocInfo.allocationSize = AlignUp(curOffset, (VkDeviceSize)1024ULL);

    // allocate one big block
    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &m_TexDisplayDummyMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // bind all the image memory
    for(index = 0; index < (int)ARRAY_COUNT(m_TexDisplayDummyImages); index++)
    {
      vkr = m_pDriver->vkBindImageMemory(dev, m_TexDisplayDummyImages[index],
                                         m_TexDisplayDummyMemory, offsets[index]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    // now that the image memory is bound, we can create the image views and fill the descriptor set
    // writes.
    index = 0;
    for(size_t fmt = 0; fmt < ARRAY_COUNT(formats); fmt++)
    {
      for(size_t type = 0; type < ARRAY_COUNT(types); type++)
      {
        VkImageViewCreateInfo viewInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            NULL,
            0,
            m_TexDisplayDummyImages[index],
            viewtypes[type],
            formats[fmt],
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
            },
        };

        vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &m_TexDisplayDummyImageViews[index]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        m_TexDisplayDummyInfos[index].imageView = Unwrap(m_TexDisplayDummyImageViews[index]);

        // need to update image layout into valid state
        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
            Unwrap(m_TexDisplayDummyImages[index]),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

        DoPipelineBarrier(replayDataCmd, 1, &barrier);

        index++;
      }
    }
  }

  m_OverdrawRampUBO.Create(driver, dev, 2048, 1, 0);    // no ring needed, fixed data
  RDCCOMPILE_ASSERT(sizeof(overdrawRamp) <= 2048, "overdraw ramp uniforms size");

  void *ramp = m_OverdrawRampUBO.Map();
  memcpy(ramp, overdrawRamp, sizeof(overdrawRamp));
  m_OverdrawRampUBO.Unmap();

  m_TriSizeUBO.Create(driver, dev, sizeof(Vec4f), 4096, 0);

  // pick pixel data
  {
    // create image

    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        {1, 1, 1},
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vkr = m_pDriver->vkCreateImage(dev, &imInfo, NULL, &m_PickPixelImage);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetImageMemoryRequirements(dev, m_PickPixelImage, &mrq);

    // allocate readback memory
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &m_PickPixelImageMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindImageMemory(dev, m_PickPixelImage, m_PickPixelImageMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        m_PickPixelImage,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
        },
    };

    vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &m_PickPixelImageView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // need to update image layout into valid state

    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
        Unwrap(m_PickPixelImage),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(replayDataCmd, 1, &barrier);

    // create render pass
    VkAttachmentDescription attDesc = {0,
                                       VK_FORMAT_R32G32B32A32_SFLOAT,
                                       VK_SAMPLE_COUNT_1_BIT,
                                       VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_ATTACHMENT_STORE_OP_STORE,
                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                       VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub = {
        0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,    NULL,       // inputs
        1,    &attRef,    // color
        NULL,             // resolve
        NULL,             // depth-stencil
        0,    NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        1,
        &attDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    vkr = m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &m_PickPixelRP);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // create framebuffer
    VkFramebufferCreateInfo fbinfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        m_PickPixelRP,
        1,
        &m_PickPixelImageView,
        1,
        1,
        1,
    };

    vkr = m_pDriver->vkCreateFramebuffer(dev, &fbinfo, NULL, &m_PickPixelFB);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // since we always sync for readback, doesn't need to be ring'd
    m_PickPixelReadbackBuffer.Create(driver, dev, sizeof(float) * 4, 1,
                                     GPUBuffer::eGPUBufferReadback);
  }

  m_MeshUBO.Create(driver, dev, sizeof(MeshUBOData), 16, 0);
  m_MeshBBoxVB.Create(driver, dev, sizeof(Vec4f) * 128, 16, GPUBuffer::eGPUBufferVBuffer);

  Vec4f TLN = Vec4f(-1.0f, 1.0f, 0.0f, 1.0f);    // TopLeftNear, etc...
  Vec4f TRN = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
  Vec4f BLN = Vec4f(-1.0f, -1.0f, 0.0f, 1.0f);
  Vec4f BRN = Vec4f(1.0f, -1.0f, 0.0f, 1.0f);

  Vec4f TLF = Vec4f(-1.0f, 1.0f, 1.0f, 1.0f);
  Vec4f TRF = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
  Vec4f BLF = Vec4f(-1.0f, -1.0f, 1.0f, 1.0f);
  Vec4f BRF = Vec4f(1.0f, -1.0f, 1.0f, 1.0f);

  Vec4f axisFrustum[] = {
      // axis marker vertices
      Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
      Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f),

      // frustum vertices
      TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

      TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

      TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
  };

  // doesn't need to be ring'd as it's immutable
  m_MeshAxisFrustumVB.Create(driver, dev, sizeof(axisFrustum), 1, GPUBuffer::eGPUBufferVBuffer);

  Vec4f *axisData = (Vec4f *)m_MeshAxisFrustumVB.Map();

  memcpy(axisData, axisFrustum, sizeof(axisFrustum));

  m_MeshAxisFrustumVB.Unmap();

  const uint32_t maxTexDim = 16384;
  const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
  const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

  const size_t byteSize =
      2 * sizeof(Vec4f) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;

  m_MinMaxTileResult.Create(driver, dev, byteSize, 1, GPUBuffer::eGPUBufferSSBO);
  m_MinMaxResult.Create(driver, dev, sizeof(Vec4f) * 2, 1, GPUBuffer::eGPUBufferSSBO);
  m_MinMaxReadback.Create(driver, dev, sizeof(Vec4f) * 2, 1, GPUBuffer::eGPUBufferReadback);
  m_HistogramBuf.Create(driver, dev, sizeof(uint32_t) * 4 * HGRAM_NUM_BUCKETS, 1,
                        GPUBuffer::eGPUBufferSSBO);
  m_HistogramReadback.Create(driver, dev, sizeof(uint32_t) * 4 * HGRAM_NUM_BUCKETS, 1,
                             GPUBuffer::eGPUBufferReadback);

  // don't need to ring this, as we hard-sync for readback anyway
  m_HistogramUBO.Create(driver, dev, sizeof(HistogramUBOData), 1, 0);

  ObjDisp(replayDataCmd)->EndCommandBuffer(Unwrap(replayDataCmd));

  // tex display descriptors are updated right before rendering,
  // so we don't have to update them here

  m_CheckerboardUBO.FillDescriptor(bufInfo[0]);
  m_MeshUBO.FillDescriptor(bufInfo[1]);
  m_OutlineUBO.FillDescriptor(bufInfo[2]);
  m_OverdrawRampUBO.FillDescriptor(bufInfo[3]);
  m_MeshPickUBO.FillDescriptor(bufInfo[4]);
  m_MeshPickResult.FillDescriptor(bufInfo[5]);

  VkWriteDescriptorSet analysisSetWrites[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_CheckerboardDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &bufInfo[0], NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MeshDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &bufInfo[1], NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_OutlineDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &bufInfo[2], NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_QuadDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufInfo[3], NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MeshPickDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufInfo[4], NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MeshPickDescSet), 3, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufInfo[5], NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_TriSizeDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufInfo[3], NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(analysisSetWrites), analysisSetWrites,
                                     0, NULL);
}

VulkanDebugManager::~VulkanDebugManager()
{
  VkDevice dev = m_Device;

  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("vkshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    ShaderCacheCallbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      ShaderCacheCallbacks.Destroy(it->second);
  }

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.buf, NULL);
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.idxBuf, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.bufmem, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.idxBufMem, NULL);
  }

  m_PostVSData.clear();

  // since we don't have properly registered resources, releasing our descriptor
  // pool here won't remove the descriptor sets, so we need to free our own
  // tracking data (not the API objects) for descriptor sets.

  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(uint32_t i = 0; i < MeshDisplayPipelines::ePipe_Count; i++)
      m_pDriver->vkDestroyPipeline(dev, it->second.pipes[i], NULL);

  for(size_t i = 0; i < ARRAY_COUNT(m_MeshModules); i++)
    m_pDriver->vkDestroyShaderModule(dev, m_MeshModules[i], NULL);

  m_pDriver->vkDestroyShaderModule(dev, m_TriSizeGSModule, NULL);
  m_pDriver->vkDestroyShaderModule(dev, m_TriSizeFSModule, NULL);

  m_pDriver->vkDestroyDescriptorPool(dev, m_DescriptorPool, NULL);

  m_pDriver->vkDestroySampler(dev, m_LinearSampler, NULL);
  m_pDriver->vkDestroySampler(dev, m_PointSampler, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_CheckerboardDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_CheckerboardPipeLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_CheckerboardPipeline, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_CheckerboardMSAAPipeline, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_TexDisplayDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_TexDisplayPipeLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_TexDisplayPipeline, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_TexDisplayBlendPipeline, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_TexDisplayF32Pipeline, NULL);

  for(size_t i = 0; i < ARRAY_COUNT(m_TexDisplayDummyImages); i++)
  {
    m_pDriver->vkDestroyImageView(dev, m_TexDisplayDummyImageViews[i], NULL);
    m_pDriver->vkDestroyImage(dev, m_TexDisplayDummyImages[i], NULL);
  }

  m_pDriver->vkFreeMemory(dev, m_TexDisplayDummyMemory, NULL);

  m_pDriver->vkDestroyRenderPass(dev, m_CustomTexRP, NULL);
  m_pDriver->vkDestroyFramebuffer(dev, m_CustomTexFB, NULL);
  m_pDriver->vkDestroyImage(dev, m_CustomTexImg, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_CustomTexImgView); i++)
    m_pDriver->vkDestroyImageView(dev, m_CustomTexImgView[i], NULL);
  m_pDriver->vkFreeMemory(dev, m_CustomTexMem, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_CustomTexPipeline, NULL);

  m_CheckerboardUBO.Destroy();
  m_TexDisplayUBO.Destroy();

  m_PickPixelReadbackBuffer.Destroy();

  m_pDriver->vkDestroyFramebuffer(dev, m_PickPixelFB, NULL);
  m_pDriver->vkDestroyRenderPass(dev, m_PickPixelRP, NULL);
  m_pDriver->vkDestroyImageView(dev, m_PickPixelImageView, NULL);
  m_pDriver->vkDestroyImage(dev, m_PickPixelImage, NULL);
  m_pDriver->vkFreeMemory(dev, m_PickPixelImageMem, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_ArrayMSDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_ArrayMSPipeLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_Array2MSPipe, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_MS2ArrayPipe, NULL);

  for(size_t i = 0; i < ARRAY_COUNT(m_DepthMS2ArrayPipe); i++)
    m_pDriver->vkDestroyPipeline(dev, m_DepthMS2ArrayPipe[i], NULL);

  for(size_t f = 0; f < ARRAY_COUNT(m_DepthArray2MSPipe); f++)
    for(size_t s = 0; s < ARRAY_COUNT(m_DepthArray2MSPipe[0]); s++)
      m_pDriver->vkDestroyPipeline(dev, m_DepthArray2MSPipe[f][s], NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_TextDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_TextPipeLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_TextPipeline); i++)
    m_pDriver->vkDestroyPipeline(dev, m_TextPipeline[i], NULL);

  m_TextGeneralUBO.Destroy();
  m_TextGlyphUBO.Destroy();
  m_TextStringUBO.Destroy();
  m_TextAtlasUpload.Destroy();

  m_pDriver->vkDestroyImageView(dev, m_TextAtlasView, NULL);
  m_pDriver->vkDestroyImage(dev, m_TextAtlas, NULL);
  m_pDriver->vkFreeMemory(dev, m_TextAtlasMem, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_MeshDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_MeshPipeLayout, NULL);

  m_MeshUBO.Destroy();
  m_MeshBBoxVB.Destroy();
  m_MeshAxisFrustumVB.Destroy();

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_OutlineDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_OutlinePipeLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_OutlinePipeline); i++)
    m_pDriver->vkDestroyPipeline(dev, m_OutlinePipeline[i], NULL);

  m_OutlineUBO.Destroy();

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_HistogramDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_HistogramPipeLayout, NULL);

  for(size_t t = 1; t < eTexType_Max; t++)
  {
    for(size_t f = 0; f < 3; f++)
    {
      m_pDriver->vkDestroyPipeline(dev, m_MinMaxTilePipe[t][f], NULL);
      m_pDriver->vkDestroyPipeline(dev, m_HistogramPipe[t][f], NULL);
      if(t == 1)
        m_pDriver->vkDestroyPipeline(dev, m_MinMaxResultPipe[f], NULL);
    }
  }

  m_ReadbackWindow.Destroy();

  m_MinMaxTileResult.Destroy();
  m_MinMaxResult.Destroy();
  m_MinMaxReadback.Destroy();
  m_HistogramBuf.Destroy();
  m_HistogramReadback.Destroy();
  m_HistogramUBO.Destroy();

  m_OverdrawRampUBO.Destroy();

  m_MeshPickUBO.Destroy();
  m_MeshPickIB.Destroy();
  m_MeshPickIBUpload.Destroy();
  m_MeshPickVB.Destroy();
  m_MeshPickVBUpload.Destroy();
  m_MeshPickResult.Destroy();
  m_MeshPickResultReadback.Destroy();

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_MeshPickDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_MeshPickLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_MeshPickPipeline, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_MeshFetchDescSetLayout, NULL);
  m_pDriver->vkDestroyFramebuffer(dev, m_OverlayNoDepthFB, NULL);
  m_pDriver->vkDestroyRenderPass(dev, m_OverlayNoDepthRP, NULL);
  m_pDriver->vkDestroyImageView(dev, m_OverlayImageView, NULL);
  m_pDriver->vkDestroyImage(dev, m_OverlayImage, NULL);
  m_pDriver->vkFreeMemory(dev, m_OverlayImageMem, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_TriSizeDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_TriSizePipeLayout, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_QuadDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_QuadResolvePipeLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_QuadResolvePipeline); i++)
    m_pDriver->vkDestroyPipeline(dev, m_QuadResolvePipeline[i], NULL);
}

void VulkanDebugManager::BeginText(const TextPrintState &textstate)
{
  VkClearValue clearval = {};
  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(textstate.rp),
      Unwrap(textstate.fb),
      {{
           0, 0,
       },
       {textstate.w, textstate.h}},
      1,
      &clearval,
  };
  ObjDisp(textstate.cmd)->CmdBeginRenderPass(Unwrap(textstate.cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

  // assuming VK_FORMAT_R8G8B8A8_SRGB as default

  VkPipeline pipe = m_TextPipeline[0];

  if(textstate.fmt == VK_FORMAT_R8G8B8A8_UNORM)
    pipe = m_TextPipeline[1];
  else if(textstate.fmt == VK_FORMAT_B8G8R8A8_SRGB)
    pipe = m_TextPipeline[2];
  else if(textstate.fmt == VK_FORMAT_B8G8R8A8_UNORM)
    pipe = m_TextPipeline[3];

  ObjDisp(textstate.cmd)
      ->CmdBindPipeline(Unwrap(textstate.cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

  VkViewport viewport = {0.0f, 0.0f, (float)textstate.w, (float)textstate.h, 0.0f, 1.0f};
  ObjDisp(textstate.cmd)->CmdSetViewport(Unwrap(textstate.cmd), 0, 1, &viewport);
}

void VulkanDebugManager::RenderText(const TextPrintState &textstate, float x, float y,
                                    const char *textfmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, textfmt);
  StringFormat::vsnprintf(tmpBuf, 4095, textfmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderTextInternal(textstate, x, y, tmpBuf);
}

void VulkanDebugManager::RenderTextInternal(const TextPrintState &textstate, float x, float y,
                                            const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderTextInternal(textstate, x, y, text);
    RenderTextInternal(textstate, x, y + 1.0f, t + 1);
    *t = '\n';
    return;
  }

  if(strlen(text) == 0)
    return;

  uint32_t offsets[2] = {0};

  FontUBOData *ubo = (FontUBOData *)m_TextGeneralUBO.Map(&offsets[0]);

  ubo->TextPosition.x = x;
  ubo->TextPosition.y = y;

  ubo->FontScreenAspect.x = 1.0f / float(textstate.w);
  ubo->FontScreenAspect.y = 1.0f / float(textstate.h);

  ubo->TextSize = m_FontCharSize;
  ubo->FontScreenAspect.x *= m_FontCharAspect;

  ubo->CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
  ubo->CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

  m_TextGeneralUBO.Unmap();

  size_t len = strlen(text);

  RDCASSERT(len <= MAX_SINGLE_LINE_LENGTH);

  // only map enough for our string
  StringUBOData *stringData = (StringUBOData *)m_TextStringUBO.Map(&offsets[1], len * sizeof(Vec4u));

  for(size_t i = 0; i < strlen(text); i++)
    stringData->chars[i].x = uint32_t(text[i] - ' ');

  m_TextStringUBO.Unmap();

  ObjDisp(textstate.cmd)
      ->CmdBindDescriptorSets(Unwrap(textstate.cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_TextPipeLayout), 0, 1, UnwrapPtr(m_TextDescSet), 2, offsets);

  ObjDisp(textstate.cmd)->CmdDraw(Unwrap(textstate.cmd), 6 * (uint32_t)strlen(text), 1, 0, 0);
}

void VulkanDebugManager::ReplaceResource(ResourceId from, ResourceId to)
{
  VkDevice dev = m_pDriver->GetDev();

  // we're passed in the original ID but we want the live ID for comparison
  ResourceId liveid = GetResourceManager()->GetLiveID(from);

  VkShaderModule srcShaderModule = GetResourceManager()->GetCurrentHandle<VkShaderModule>(liveid);
  VkShaderModule dstShaderModule = GetResourceManager()->GetCurrentHandle<VkShaderModule>(to);

  // remake and replace any pipelines that referenced this shader
  for(auto it = m_pDriver->m_CreationInfo.m_Pipeline.begin();
      it != m_pDriver->m_CreationInfo.m_Pipeline.end(); ++it)
  {
    bool refdShader = false;
    for(size_t i = 0; i < ARRAY_COUNT(it->second.shaders); i++)
    {
      if(it->second.shaders[i].module == liveid)
      {
        refdShader = true;
        break;
      }
    }

    if(refdShader)
    {
      VkPipeline pipe = VK_NULL_HANDLE;
      const VulkanCreationInfo::Pipeline &pipeInfo = m_pDriver->m_CreationInfo.m_Pipeline[it->first];
      if(pipeInfo.renderpass != ResourceId())    // check if this is a graphics or compute pipeline
      {
        VkGraphicsPipelineCreateInfo pipeCreateInfo;
        MakeGraphicsPipelineInfo(pipeCreateInfo, it->first);

        // replace the relevant module
        for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
        {
          VkPipelineShaderStageCreateInfo &sh =
              (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];

          if(sh.module == srcShaderModule)
            sh.module = dstShaderModule;
        }

        // create the new graphics pipeline
        VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                            NULL, &pipe);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }
      else
      {
        VkComputePipelineCreateInfo pipeCreateInfo;
        MakeComputePipelineInfo(pipeCreateInfo, it->first);

        // replace the relevant module
        VkPipelineShaderStageCreateInfo &sh = pipeCreateInfo.stage;
        RDCASSERT(sh.module == srcShaderModule);
        sh.module = dstShaderModule;

        // create the new compute pipeline
        VkResult vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                           NULL, &pipe);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      // remove the replacements
      GetResourceManager()->ReplaceResource(it->first, GetResID(pipe));
      GetResourceManager()->ReplaceResource(GetResourceManager()->GetOriginalID(it->first),
                                            GetResID(pipe));
    }
  }

  // make the actual shader module replacements
  GetResourceManager()->ReplaceResource(from, to);
  GetResourceManager()->ReplaceResource(liveid, to);
}

void VulkanDebugManager::RemoveReplacement(ResourceId id)
{
  VkDevice dev = m_pDriver->GetDev();

  // we're passed in the original ID but we want the live ID for comparison
  ResourceId liveid = GetResourceManager()->GetLiveID(id);

  if(!GetResourceManager()->HasReplacement(id))
    return;

  // remove the actual shader module replacements
  GetResourceManager()->RemoveReplacement(id);
  GetResourceManager()->RemoveReplacement(liveid);

  // remove any replacements on pipelines that referenced this shader
  for(auto it = m_pDriver->m_CreationInfo.m_Pipeline.begin();
      it != m_pDriver->m_CreationInfo.m_Pipeline.end(); ++it)
  {
    bool refdShader = false;
    for(size_t i = 0; i < ARRAY_COUNT(it->second.shaders); i++)
    {
      if(it->second.shaders[i].module == liveid)
      {
        refdShader = true;
        break;
      }
    }

    if(refdShader)
    {
      VkPipeline pipe = GetResourceManager()->GetCurrentHandle<VkPipeline>(it->first);

      // delete the replacement pipeline
      m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

      // remove both live and original replacements, since we will have made these above
      GetResourceManager()->RemoveReplacement(it->first);
      GetResourceManager()->RemoveReplacement(GetResourceManager()->GetOriginalID(it->first));
    }
  }
}

void VulkanDebugManager::CreateCustomShaderTex(uint32_t width, uint32_t height, uint32_t mip)
{
  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  if(m_CustomTexImg != VK_NULL_HANDLE)
  {
    if(width == m_CustomTexWidth && height == m_CustomTexHeight)
    {
      // recreate framebuffer for this mip

      // Create framebuffer rendering just to overlay image, no depth
      VkFramebufferCreateInfo fbinfo = {
          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          NULL,
          0,
          m_CustomTexRP,
          1,
          &m_CustomTexImgView[mip],
          RDCMAX(1U, width >> mip),
          RDCMAX(1U, height >> mip),
          1,
      };

      vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_CustomTexFB);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
      return;
    }

    m_pDriver->vkDestroyRenderPass(dev, m_CustomTexRP, NULL);
    m_pDriver->vkDestroyFramebuffer(dev, m_CustomTexFB, NULL);
    for(size_t i = 0; i < ARRAY_COUNT(m_CustomTexImgView); i++)
      m_pDriver->vkDestroyImageView(dev, m_CustomTexImgView[i], NULL);
    RDCEraseEl(m_CustomTexImgView);
    m_pDriver->vkDestroyImage(dev, m_CustomTexImg, NULL);
  }

  m_CustomTexWidth = width;
  m_CustomTexHeight = height;

  VkImageCreateInfo imInfo = {
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      NULL,
      0,
      VK_IMAGE_TYPE_2D,
      VK_FORMAT_R16G16B16A16_SFLOAT,
      {width, height, 1},
      CalcNumMips((int)width, (int)height, 1),
      1,
      VK_SAMPLE_COUNT_1_BIT,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0,
      NULL,
      VK_IMAGE_LAYOUT_UNDEFINED,
  };

  vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, NULL, &m_CustomTexImg);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(m_Device, m_CustomTexImg, &mrq);

  // if no memory is allocated, or it's not enough,
  // then allocate
  if(m_CustomTexMem == VK_NULL_HANDLE || mrq.size > m_CustomTexMemSize)
  {
    if(m_CustomTexMem != VK_NULL_HANDLE)
      m_pDriver->vkFreeMemory(m_Device, m_CustomTexMem, NULL);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &m_CustomTexMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_CustomTexMemSize = mrq.size;
  }

  vkr = m_pDriver->vkBindImageMemory(m_Device, m_CustomTexImg, m_CustomTexMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      m_CustomTexImg,
      VK_IMAGE_VIEW_TYPE_2D,
      imInfo.format,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
      },
  };

  for(uint32_t i = 0; i < imInfo.mipLevels; i++)
  {
    viewInfo.subresourceRange.baseMipLevel = i;
    vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &m_CustomTexImgView[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  // need to update image layout into valid state

  VkImageMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      0,
      0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
      Unwrap(m_CustomTexImg),
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1}};

  m_pDriver->m_ImageLayouts[GetResID(m_CustomTexImg)].subresourceStates[0].newLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  DoPipelineBarrier(cmd, 1, &barrier);

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  VkAttachmentDescription colDesc = {0,
                                     imInfo.format,
                                     imInfo.samples,
                                     VK_ATTACHMENT_LOAD_OP_LOAD,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                     VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription sub = {
      0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
      0,    NULL,       // inputs
      1,    &colRef,    // color
      NULL,             // resolve
      NULL,             // depth-stencil
      0,    NULL,       // preserve
  };

  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      NULL,
      0,
      1,
      &colDesc,
      1,
      &sub,
      0,
      NULL,    // dependencies
  };

  vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &m_CustomTexRP);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // Create framebuffer rendering just to overlay image, no depth
  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      NULL,
      0,
      m_CustomTexRP,
      1,
      &m_CustomTexImgView[mip],
      RDCMAX(1U, width >> mip),
      RDCMAX(1U, height >> mip),
      1,
  };

  vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_CustomTexFB);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanDebugManager::CreateCustomShaderPipeline(ResourceId shader)
{
  VkDevice dev = m_Device;

  if(shader == ResourceId())
    return;

  if(m_CustomTexPipeline != VK_NULL_HANDLE)
  {
    if(m_CustomTexShader == shader)
      return;

    m_pDriver->vkDestroyPipeline(dev, m_CustomTexPipeline, NULL);
  }

  m_CustomTexShader = shader;

  // declare the pipeline creation info and all of its sub-structures
  // these are modified as appropriate for each pipeline we create
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
       m_BlitVSModule, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
       GetResourceManager()->GetCurrentHandle<VkShaderModule>(shader), "main", NULL},
  };

  VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      NULL,
      0,
      0,
      NULL,    // vertex bindings
      0,
      NULL,    // vertex attributes
  };

  VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      NULL,
      0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      false,
  };

  VkRect2D scissor = {{0, 0}, {16384, 16384}};

  VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, &scissor};

  VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      NULL,
      0,
      true,
      false,
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_NONE,
      VK_FRONT_FACE_CLOCKWISE,
      false,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
  };

  VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      NULL,
      0,
      VK_SAMPLE_COUNT_1_BIT,
      false,
      0.0f,
      NULL,
      false,
      false,
  };

  VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      false,
      VK_COMPARE_OP_ALWAYS,
      false,
      false,
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      0.0f,
      1.0f,
  };

  VkPipelineColorBlendAttachmentState attState = {
      false,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      0xf,
  };

  VkPipelineColorBlendStateCreateInfo cb = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      VK_LOGIC_OP_NO_OP,
      1,
      &attState,
      {1.0f, 1.0f, 1.0f, 1.0f}};

  VkDynamicState dynstates[] = {VK_DYNAMIC_STATE_VIEWPORT};

  VkPipelineDynamicStateCreateInfo dyn = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      NULL,
      0,
      ARRAY_COUNT(dynstates),
      dynstates,
  };

  VkGraphicsPipelineCreateInfo pipeInfo = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      NULL,
      0,
      2,
      stages,
      &vi,
      &ia,
      NULL,    // tess
      &vp,
      &rs,
      &msaa,
      &ds,
      &cb,
      &dyn,
      m_TexDisplayPipeLayout,
      m_CustomTexRP,
      0,                 // sub pass
      VK_NULL_HANDLE,    // base pipeline handle
      -1,                // base pipeline index
  };

  VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                                      &m_CustomTexPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanDebugManager::CopyTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent,
                                            uint32_t layers, uint32_t samples, VkFormat fmt)
{
  if(!m_pDriver->GetDeviceFeatures().shaderStorageImageMultisample ||
     !m_pDriver->GetDeviceFeatures().shaderStorageImageWriteWithoutFormat)
    return;

  if(m_MS2ArrayPipe == VK_NULL_HANDLE)
    return;

  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthTex2DMSToArray(destArray, srcMS, extent, layers, samples, fmt);
    return;
  }

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcView, destView;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcMS,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_FORMAT_UNDEFINED,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  uint32_t bs = GetByteSize(1, 1, 1, fmt, 0);

  if(bs == 1)
    viewInfo.format = VK_FORMAT_R8_UINT;
  else if(bs == 2)
    viewInfo.format = VK_FORMAT_R16_UINT;
  else if(bs == 4)
    viewInfo.format = VK_FORMAT_R32_UINT;
  else if(bs == 8)
    viewInfo.format = VK_FORMAT_R32G32_UINT;
  else if(bs == 16)
    viewInfo.format = VK_FORMAT_R32G32B32A32_UINT;

  if(viewInfo.format == VK_FORMAT_UNDEFINED)
  {
    RDCERR("Can't copy 2D to Array with format %s", ToStr::Get(fmt).c_str());
    return;
  }

  if(IsStencilOnlyFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
  else if(IsDepthOrStencilFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = destArray;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkDescriptorImageInfo srcdesc = {0};
  srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc.imageView = srcView;
  srcdesc.sampler = Unwrap(m_LinearSampler);    // not used

  VkDescriptorImageInfo destdesc = {0};
  destdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  destdesc.imageView = destView;
  destdesc.sampler = Unwrap(m_LinearSampler);    // not used

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc, NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &destdesc, NULL, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_MS2ArrayPipe));
  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                      UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

  Vec4u params = {samples, 0, 0, 0};

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL, 0,
                                 sizeof(Vec4u), &params);

  ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), extent.width, extent.height, layers * samples);

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcView, NULL);
  ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView, NULL);
}

void VulkanDebugManager::CopyDepthTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent,
                                                 uint32_t layers, uint32_t samples, VkFormat fmt)
{
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

  int pipeIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM: pipeIndex = 0; break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      pipeIndex = 1;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: pipeIndex = 2; break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      pipeIndex = 3;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D32_SFLOAT: pipeIndex = 4; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      pipeIndex = 5;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default: RDCERR("Unexpected depth format: %d", fmt); return;
  }

  VkPipeline pipe = m_DepthMS2ArrayPipe[pipeIndex];

  if(pipe == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcDepthView = VK_NULL_HANDLE, srcStencilView = VK_NULL_HANDLE;
  VkImageView *destView = new VkImageView[layers * samples];

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcMS,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      fmt,
      {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
       VK_COMPONENT_SWIZZLE_ZERO},
      {
          VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcDepthView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcStencilView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.image = destArray;

  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  for(uint32_t i = 0; i < layers * samples; i++)
  {
    viewInfo.subresourceRange.baseArrayLayer = i;
    viewInfo.subresourceRange.layerCount = 1;

    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkDescriptorImageInfo srcdesc[2];
  srcdesc[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[0].imageView = srcDepthView;
  srcdesc[0].sampler = Unwrap(m_LinearSampler);    // not used
  srcdesc[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[1].imageView = srcStencilView;
  srcdesc[1].sampler = Unwrap(m_LinearSampler);    // not used

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[0], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[1], NULL, NULL},
  };

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 2, writeSet, 0, NULL);
  else
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 1, writeSet, 0, NULL);

  // create a bespoke framebuffer and renderpass for rendering
  VkAttachmentDescription attDesc = {0,
                                     fmt,
                                     VK_SAMPLE_COUNT_1_BIT,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     VK_IMAGE_LAYOUT_GENERAL};

  VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_GENERAL};

  VkSubpassDescription sub = {};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.pDepthStencilAttachment = &attRef;

  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      NULL,
      0,
      1,
      &attDesc,
      1,
      &sub,
      0,
      NULL,    // dependencies
  };

  VkRenderPass rp = VK_NULL_HANDLE;

  ObjDisp(dev)->CreateRenderPass(Unwrap(dev), &rpinfo, NULL, &rp);

  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      NULL,
      0,
      rp,
      1,
      NULL,
      extent.width,
      extent.height,
      1,
  };

  VkFramebuffer *fb = new VkFramebuffer[layers * samples];

  for(uint32_t i = 0; i < layers * samples; i++)
  {
    fbinfo.pAttachments = destView + i;

    vkr = ObjDisp(dev)->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &fb[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  VkClearValue clearval = {};

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp,        VK_NULL_HANDLE,
      {{0, 0}, {extent.width, extent.height}},  1,    &clearval,
  };

  uint32_t numStencil = 1;

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    numStencil = 256;

  Vec4u params;
  params.x = samples;

  for(uint32_t i = 0; i < layers * samples; i++)
  {
    rpbegin.framebuffer = fb[i];

    ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                        UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

    VkViewport viewport = {0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    params.y = i % samples;    // currentSample;
    params.z = i / samples;    // currentSlice;

    for(uint32_t s = 0; s < numStencil; s++)
    {
      params.w = numStencil == 1 ? 1000 : s;    // currentStencil;

      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FRONT_AND_BACK, s);
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL,
                                     0, sizeof(Vec4u), &params);
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
    }

    ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
  }

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  for(uint32_t i = 0; i < layers * samples; i++)
    ObjDisp(dev)->DestroyFramebuffer(Unwrap(dev), fb[i], NULL);
  ObjDisp(dev)->DestroyRenderPass(Unwrap(dev), rp, NULL);

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcDepthView, NULL);
  if(srcStencilView != VK_NULL_HANDLE)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcStencilView, NULL);
  for(uint32_t i = 0; i < layers * samples; i++)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView[i], NULL);

  SAFE_DELETE_ARRAY(destView);
  SAFE_DELETE_ARRAY(fb);
}

void VulkanDebugManager::CopyArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent,
                                            uint32_t layers, uint32_t samples, VkFormat fmt)
{
  if(!m_pDriver->GetDeviceFeatures().shaderStorageImageMultisample ||
     !m_pDriver->GetDeviceFeatures().shaderStorageImageWriteWithoutFormat)
    return;

  if(m_Array2MSPipe == VK_NULL_HANDLE)
    return;

  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthArrayToTex2DMS(destMS, srcArray, extent, layers, samples, fmt);
    return;
  }

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcView, destView;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcArray,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_FORMAT_UNDEFINED,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  uint32_t bs = GetByteSize(1, 1, 1, fmt, 0);

  if(bs == 1)
    viewInfo.format = VK_FORMAT_R8_UINT;
  else if(bs == 2)
    viewInfo.format = VK_FORMAT_R16_UINT;
  else if(bs == 4)
    viewInfo.format = VK_FORMAT_R32_UINT;
  else if(bs == 8)
    viewInfo.format = VK_FORMAT_R32G32_UINT;
  else if(bs == 16)
    viewInfo.format = VK_FORMAT_R32G32B32A32_UINT;

  if(viewInfo.format == VK_FORMAT_UNDEFINED)
  {
    RDCERR("Can't copy Array to MS with format %s", ToStr::Get(fmt).c_str());
    return;
  }

  if(IsStencilOnlyFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
  else if(IsDepthOrStencilFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = destMS;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkDescriptorImageInfo srcdesc = {0};
  srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc.imageView = srcView;
  srcdesc.sampler = Unwrap(m_LinearSampler);    // not used

  VkDescriptorImageInfo destdesc = {0};
  destdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  destdesc.imageView = destView;
  destdesc.sampler = Unwrap(m_LinearSampler);    // not used

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc, NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &destdesc, NULL, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_Array2MSPipe));
  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                      UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

  Vec4u params = {samples, 0, 0, 0};

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL, 0,
                                 sizeof(Vec4u), &params);

  ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), extent.width, extent.height, layers * samples);

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcView, NULL);
  ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView, NULL);
}

void VulkanDebugManager::CopyDepthArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent,
                                                 uint32_t layers, uint32_t samples, VkFormat fmt)
{
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

  int pipeIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM: pipeIndex = 0; break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      pipeIndex = 1;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: pipeIndex = 2; break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      pipeIndex = 3;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D32_SFLOAT: pipeIndex = 4; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      pipeIndex = 5;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default: RDCERR("Unexpected depth format: %d", fmt); return;
  }

  // 0-based from 2x MSAA
  uint32_t sampleIndex = SampleIndex((VkSampleCountFlagBits)samples) - 1;

  if(sampleIndex >= ARRAY_COUNT(m_DepthArray2MSPipe[0]))
  {
    RDCERR("Unsupported sample count %u", samples);
    return;
  }

  VkPipeline pipe = m_DepthArray2MSPipe[pipeIndex][sampleIndex];

  if(pipe == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcDepthView = VK_NULL_HANDLE, srcStencilView = VK_NULL_HANDLE;
  VkImageView *destView = new VkImageView[layers];

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcArray,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      fmt,
      {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
       VK_COMPONENT_SWIZZLE_ZERO},
      {
          VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcDepthView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcStencilView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.image = destMS;

  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  for(uint32_t i = 0; i < layers; i++)
  {
    viewInfo.subresourceRange.baseArrayLayer = i;
    viewInfo.subresourceRange.layerCount = 1;

    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkDescriptorImageInfo srcdesc[2];
  srcdesc[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[0].imageView = srcDepthView;
  srcdesc[0].sampler = Unwrap(m_LinearSampler);    // not used
  srcdesc[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[1].imageView = srcStencilView;
  srcdesc[1].sampler = Unwrap(m_LinearSampler);    // not used

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[0], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[1], NULL, NULL},
  };

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 2, writeSet, 0, NULL);
  else
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 1, writeSet, 0, NULL);

  // create a bespoke framebuffer and renderpass for rendering
  VkAttachmentDescription attDesc = {0,
                                     fmt,
                                     (VkSampleCountFlagBits)samples,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     VK_IMAGE_LAYOUT_GENERAL};

  VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_GENERAL};

  VkSubpassDescription sub = {};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.pDepthStencilAttachment = &attRef;

  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      NULL,
      0,
      1,
      &attDesc,
      1,
      &sub,
      0,
      NULL,    // dependencies
  };

  VkRenderPass rp = VK_NULL_HANDLE;

  ObjDisp(dev)->CreateRenderPass(Unwrap(dev), &rpinfo, NULL, &rp);

  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      NULL,
      0,
      rp,
      1,
      NULL,
      extent.width,
      extent.height,
      1,
  };

  VkFramebuffer *fb = new VkFramebuffer[layers];

  for(uint32_t i = 0; i < layers; i++)
  {
    fbinfo.pAttachments = destView + i;

    vkr = ObjDisp(dev)->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &fb[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  VkClearValue clearval = {};

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp,        VK_NULL_HANDLE,
      {{0, 0}, {extent.width, extent.height}},  1,    &clearval,
  };

  uint32_t numStencil = 1;

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    numStencil = 256;

  Vec4u params;
  params.x = samples;
  params.y = 0;    // currentSample;

  for(uint32_t i = 0; i < layers; i++)
  {
    rpbegin.framebuffer = fb[i];

    ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                        UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

    VkViewport viewport = {0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    params.z = i;    // currentSlice;

    for(uint32_t s = 0; s < numStencil; s++)
    {
      params.w = numStencil == 1 ? 1000 : s;    // currentStencil;

      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FRONT_AND_BACK, s);
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL,
                                     0, sizeof(Vec4u), &params);
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
    }

    ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
  }

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  for(uint32_t i = 0; i < layers; i++)
    ObjDisp(dev)->DestroyFramebuffer(Unwrap(dev), fb[i], NULL);
  ObjDisp(dev)->DestroyRenderPass(Unwrap(dev), rp, NULL);

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcDepthView, NULL);
  if(srcStencilView != VK_NULL_HANDLE)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcStencilView, NULL);
  for(uint32_t i = 0; i < layers; i++)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView[i], NULL);

  SAFE_DELETE_ARRAY(destView);
  SAFE_DELETE_ARRAY(fb);
}

// TODO: Point meshes don't pick correctly
uint32_t VulkanDebugManager::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x,
                                        uint32_t y, uint32_t w, uint32_t h)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkLayerDispatchTable *vt = ObjDisp(dev);

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(w) / float(h));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f pickMVP = projMat.Mul(camMat);

  ResourceFormat resFmt;
  resFmt.compByteWidth = cfg.position.compByteWidth;
  resFmt.compCount = cfg.position.compCount;
  resFmt.compType = cfg.position.compType;
  resFmt.special = false;
  if(cfg.position.specialFormat != SpecialFormat::Unknown)
  {
    resFmt.special = true;
    resFmt.specialFormat = cfg.position.specialFormat;
  }

  Matrix4f pickMVPProj;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    pickMVPProj = projMat.Mul(camMat.Mul(guessProj.Inverse()));
  }

  vec3 rayPos;
  vec3 rayDir;
  // convert mouse pos to world space ray
  {
    Matrix4f inversePickMVP = pickMVP.Inverse();

    float pickX = ((float)x) / ((float)w);
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)h);
    // flip the Y axis
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    vec3 cameraToWorldNearPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

    vec3 cameraToWorldFarPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

    vec3 testDir = (cameraToWorldFarPosition - cameraToWorldNearPosition);
    testDir.Normalise();

    /* Calculate the ray direction first in the regular way (above), so we can use the
    the output for testing if the ray we are picking is negative or not. This is similar
    to checking against the forward direction of the camera, but more robust
    */
    if(cfg.position.unproject)
    {
      Matrix4f inversePickMVPGuess = pickMVPProj.Inverse();

      vec3 nearPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

      vec3 farPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else
    {
      rayDir = testDir;
      rayPos = cameraToWorldNearPosition;
    }
  }

  MeshPickUBOData *ubo = (MeshPickUBOData *)m_MeshPickUBO.Map();

  ubo->rayPos = rayPos;
  ubo->rayDir = rayDir;
  ubo->use_indices = cfg.position.idxByteWidth ? 1U : 0U;
  ubo->numVerts = cfg.position.numVerts;
  bool isTriangleMesh = true;

  switch(cfg.position.topo)
  {
    case Topology::TriangleList:
    {
      ubo->meshMode = MESH_TRIANGLE_LIST;
      break;
    };
    case Topology::TriangleStrip:
    {
      ubo->meshMode = MESH_TRIANGLE_STRIP;
      break;
    };
    case Topology::TriangleFan:
    {
      ubo->meshMode = MESH_TRIANGLE_FAN;
      break;
    };
    case Topology::TriangleList_Adj:
    {
      ubo->meshMode = MESH_TRIANGLE_LIST_ADJ;
      break;
    };
    case Topology::TriangleStrip_Adj:
    {
      ubo->meshMode = MESH_TRIANGLE_STRIP_ADJ;
      break;
    };
    default:    // points, lines, patchlists, unknown
    {
      ubo->meshMode = MESH_OTHER;
      isTriangleMesh = false;
    };
  }

  // line/point data
  ubo->unproject = cfg.position.unproject;
  ubo->mvp = cfg.position.unproject ? pickMVPProj : pickMVP;
  ubo->coords = Vec2f((float)x, (float)y);
  ubo->viewport = Vec2f((float)w, (float)h);

  m_MeshPickUBO.Unmap();

  vector<byte> idxs;

  if(cfg.position.idxByteWidth && cfg.position.idxbuf != ResourceId())
    GetBufferData(cfg.position.idxbuf, cfg.position.idxoffs, 0, idxs);

  // We copy into our own buffers to promote to the target type (uint32) that the
  // shader expects. Most IBs will be 16-bit indices, most VBs will not be float4.

  if(!idxs.empty())
  {
    // resize up on demand
    if(m_MeshPickIBSize < cfg.position.numVerts * sizeof(uint32_t))
    {
      if(m_MeshPickIBSize > 0)
      {
        m_MeshPickIB.Destroy();
        m_MeshPickIBUpload.Destroy();
      }

      m_MeshPickIBSize = cfg.position.numVerts * sizeof(uint32_t);

      m_MeshPickIB.Create(m_pDriver, dev, m_MeshPickIBSize, 1,
                          GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
      m_MeshPickIBUpload.Create(m_pDriver, dev, m_MeshPickIBSize, 1, 0);
    }

    uint32_t *outidxs = (uint32_t *)m_MeshPickIBUpload.Map();

    memset(outidxs, 0, m_MeshPickIBSize);

    uint16_t *idxs16 = (uint16_t *)&idxs[0];
    uint32_t *idxs32 = (uint32_t *)&idxs[0];

    // if indices are 16-bit, manually upcast them so the shader only
    // has to deal with one type
    if(cfg.position.idxByteWidth == 2)
    {
      size_t bufsize = idxs.size() / 2;

      for(uint32_t i = 0; i < bufsize && i < cfg.position.numVerts; i++)
        outidxs[i] = idxs16[i];
    }
    else
    {
      size_t bufsize = idxs.size() / 4;

      memcpy(outidxs, idxs32, RDCMIN(bufsize, cfg.position.numVerts * sizeof(uint32_t)));
    }

    m_MeshPickIBUpload.Unmap();
  }

  if(m_MeshPickVBSize < cfg.position.numVerts * sizeof(FloatVector))
  {
    if(m_MeshPickVBSize > 0)
    {
      m_MeshPickVB.Destroy();
      m_MeshPickVBUpload.Destroy();
    }

    m_MeshPickVBSize = cfg.position.numVerts * sizeof(FloatVector);

    m_MeshPickVB.Create(m_pDriver, dev, m_MeshPickVBSize, 1,
                        GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
    m_MeshPickVBUpload.Create(m_pDriver, dev, m_MeshPickVBSize, 1, 0);
  }

  // unpack and linearise the data
  {
    vector<byte> oldData;
    GetBufferData(cfg.position.buf, cfg.position.offset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

    FloatVector *vbData = (FloatVector *)m_MeshPickVBUpload.Map();

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numVerts; i++)
    {
      uint32_t idx = i;

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(idx < idxclamp)
        idx = 0;
      else if(cfg.position.baseVertex < 0)
        idx -= idxclamp;
      else if(cfg.position.baseVertex > 0)
        idx += cfg.position.baseVertex;

      vbData[i] = HighlightCache::InterpretVertex(data, idx, cfg, dataEnd, valid);
    }

    m_MeshPickVBUpload.Unmap();
  }

  VkDescriptorBufferInfo ibInfo = {};
  VkDescriptorBufferInfo vbInfo = {};

  m_MeshPickVB.FillDescriptor(vbInfo);
  m_MeshPickIB.FillDescriptor(ibInfo);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MeshPickDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &vbInfo, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MeshPickDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &ibInfo, NULL},
  };

  if(!idxs.empty())
    vt->UpdateDescriptorSets(Unwrap(m_Device), 2, writes, 0, NULL);
  else
    vt->UpdateDescriptorSets(Unwrap(m_Device), 1, writes, 0, NULL);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkBufferCopy bufCopy = {0, 0, 0};

  vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  // reset first uint (used as atomic counter) to 0
  vt->CmdFillBuffer(Unwrap(cmd), Unwrap(m_MeshPickResult.buf), 0, sizeof(uint32_t) * 4, 0);

  VkBufferMemoryBarrier bufBarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(m_MeshPickResult.buf),
      0,
      VK_WHOLE_SIZE,
  };

  // wait for zero to be written to atomic counter before using in shader
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  // copy uploaded VB and if needed IB
  if(!idxs.empty())
  {
    // wait for writes
    bufBarrier.buffer = Unwrap(m_MeshPickIBUpload.buf);
    bufBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    DoPipelineBarrier(cmd, 1, &bufBarrier);

    // do copy
    bufCopy.size = m_MeshPickIBSize;
    vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_MeshPickIBUpload.buf), Unwrap(m_MeshPickIB.buf), 1,
                      &bufCopy);

    // wait for copy
    bufBarrier.buffer = Unwrap(m_MeshPickIB.buf);
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
    DoPipelineBarrier(cmd, 1, &bufBarrier);
  }

  // wait for writes
  bufBarrier.buffer = Unwrap(m_MeshPickVBUpload.buf);
  bufBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  // do copy
  bufCopy.size = m_MeshPickVBSize;
  vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_MeshPickVBUpload.buf), Unwrap(m_MeshPickVB.buf), 1,
                    &bufCopy);

  // wait for copy
  bufBarrier.buffer = Unwrap(m_MeshPickVB.buf);
  bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_MeshPickPipeline));
  vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_MeshPickLayout),
                            0, 1, UnwrapPtr(m_MeshPickDescSet), 0, NULL);

  uint32_t workgroupx = uint32_t(cfg.position.numVerts / 128 + 1);
  vt->CmdDispatch(Unwrap(cmd), workgroupx, 1, 1);

  // wait for shader to finish writing before transferring to readback buffer
  bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bufBarrier.buffer = Unwrap(m_MeshPickResult.buf);
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  bufCopy.size = m_MeshPickResult.totalsize;

  // copy to readback buffer
  vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_MeshPickResult.buf), Unwrap(m_MeshPickResultReadback.buf),
                    1, &bufCopy);

  // wait for transfer to finish before reading on CPU
  bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  bufBarrier.buffer = Unwrap(m_MeshPickResultReadback.buf);
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  VkResult vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  uint32_t *pickResultData = (uint32_t *)m_MeshPickResultReadback.Map();
  uint32_t numResults = *pickResultData;

  uint32_t ret = ~0U;

  if(numResults > 0)
  {
    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        vec3 intersectionPoint;
      };

      PickResult *pickResults = (PickResult *)(pickResultData + 4);

      PickResult *closest = pickResults;
      // distance from raycast hit to nearest worldspace position of the mouse
      float closestPickDistance = (closest->intersectionPoint - rayPos).Length();

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)maxMeshPicks, numResults); i++)
      {
        float pickDistance = (pickResults[i].intersectionPoint - rayPos).Length();
        if(pickDistance < closestPickDistance)
        {
          closest = pickResults + i;
        }
      }
      ret = closest->vertid;
    }
    else
    {
      struct PickResult
      {
        uint32_t vertid;
        uint32_t idx;
        float len;
        float depth;
      };

      PickResult *pickResults = (PickResult *)(pickResultData + 4);

      PickResult *closest = pickResults;

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)maxMeshPicks, numResults); i++)
      {
        // We need to keep the picking order consistent in the face
        // of random buffer appends, when multiple vertices have the
        // identical position (e.g. if UVs or normals are different).
        //
        // We could do something to try and disambiguate, but it's
        // never going to be intuitive, it's just going to flicker
        // confusingly.
        if(pickResults[i].len < closest->len ||
           (pickResults[i].len == closest->len && pickResults[i].depth < closest->depth) ||
           (pickResults[i].len == closest->len && pickResults[i].depth == closest->depth &&
            pickResults[i].vertid < closest->vertid))
          closest = pickResults + i;
      }
      ret = closest->vertid;
    }
  }

  m_MeshPickResultReadback.Unmap();

  return ret;
}

void VulkanDebugManager::EndText(const TextPrintState &textstate)
{
  ObjDisp(textstate.cmd)->CmdEndRenderPass(Unwrap(textstate.cmd));
}

void VulkanDebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len,
                                       vector<byte> &ret)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkLayerDispatchTable *vt = ObjDisp(dev);

  VkBuffer srcBuf = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(buff);

  if(srcBuf == VK_NULL_HANDLE)
  {
    RDCERR("Getting buffer data for unknown buffer %llu!", buff);
    return;
  }

  uint64_t bufsize = m_pDriver->m_CreationInfo.m_Buffer[buff].size;

  if(offset >= bufsize)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(len == 0)
  {
    len = bufsize - offset;
  }

  if(len > 0 && VkDeviceSize(offset + len) > bufsize)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%llu)",
            offset, len, bufsize);
    len = RDCMIN(len, bufsize - offset);
  }

  ret.resize((size_t)len);

  VkDeviceSize srcoffset = (VkDeviceSize)offset;
  size_t dstoffset = 0;
  VkDeviceSize sizeRemaining = (VkDeviceSize)len;

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkBufferMemoryBarrier bufBarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      0,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(srcBuf),
      srcoffset,
      sizeRemaining,
  };

  bufBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;

  // wait for previous writes to happen before we copy to our window buffer
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  while(sizeRemaining > 0)
  {
    VkDeviceSize chunkSize = RDCMIN(sizeRemaining, STAGE_BUFFER_BYTE_SIZE);

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferCopy region = {srcoffset, 0, chunkSize};
    vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(m_ReadbackWindow.buf), 1, &region);

    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufBarrier.buffer = Unwrap(m_ReadbackWindow.buf);
    bufBarrier.offset = 0;
    bufBarrier.size = chunkSize;

    // wait for transfer to happen before we read
    DoPipelineBarrier(cmd, 1, &bufBarrier);

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    byte *pData = NULL;
    vkr = vt->MapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem), 0, VK_WHOLE_SIZE, 0,
                        (void **)&pData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    RDCASSERT(pData != NULL);
    memcpy(&ret[dstoffset], pData, (size_t)chunkSize);

    dstoffset += (size_t)chunkSize;
    sizeRemaining -= chunkSize;

    vt->UnmapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem));
  }

  vt->DeviceWaitIdle(Unwrap(dev));
}

void VulkanDebugManager::MakeGraphicsPipelineInfo(VkGraphicsPipelineCreateInfo &pipeCreateInfo,
                                                  ResourceId pipeline)
{
  const VulkanCreationInfo::Pipeline &pipeInfo = m_pDriver->m_CreationInfo.m_Pipeline[pipeline];

  static VkPipelineShaderStageCreateInfo stages[6];
  static VkSpecializationInfo specInfo[6];
  static vector<VkSpecializationMapEntry> specMapEntries;

  size_t specEntries = 0;

  for(uint32_t i = 0; i < 6; i++)
    if(pipeInfo.shaders[i].module != ResourceId())
      if(!pipeInfo.shaders[i].specialization.empty())
        specEntries += pipeInfo.shaders[i].specialization.size();

  specMapEntries.resize(specEntries);

  VkSpecializationMapEntry *entry = &specMapEntries[0];

  uint32_t stageCount = 0;

  for(uint32_t i = 0; i < 6; i++)
  {
    if(pipeInfo.shaders[i].module != ResourceId())
    {
      stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[stageCount].stage = (VkShaderStageFlagBits)(1 << i);
      stages[stageCount].module =
          GetResourceManager()->GetCurrentHandle<VkShaderModule>(pipeInfo.shaders[i].module);
      stages[stageCount].pName = pipeInfo.shaders[i].entryPoint.c_str();
      stages[stageCount].pNext = NULL;
      stages[stageCount].pSpecializationInfo = NULL;

      if(!pipeInfo.shaders[i].specialization.empty())
      {
        stages[stageCount].pSpecializationInfo = &specInfo[i];
        specInfo[i].pMapEntries = entry;
        specInfo[i].mapEntryCount = (uint32_t)pipeInfo.shaders[i].specialization.size();

        byte *minDataPtr = NULL;
        byte *maxDataPtr = NULL;

        for(size_t s = 0; s < pipeInfo.shaders[i].specialization.size(); s++)
        {
          entry[s].constantID = pipeInfo.shaders[i].specialization[s].specID;
          entry[s].size = pipeInfo.shaders[i].specialization[s].size;

          if(minDataPtr == NULL)
            minDataPtr = pipeInfo.shaders[i].specialization[s].data;
          else
            minDataPtr = RDCMIN(minDataPtr, pipeInfo.shaders[i].specialization[s].data);

          maxDataPtr = RDCMAX(minDataPtr, pipeInfo.shaders[i].specialization[s].data + entry[s].size);
        }

        for(size_t s = 0; s < pipeInfo.shaders[i].specialization.size(); s++)
          entry[s].offset = (uint32_t)(pipeInfo.shaders[i].specialization[s].data - minDataPtr);

        specInfo[i].dataSize = (maxDataPtr - minDataPtr);
        specInfo[i].pData = (const void *)minDataPtr;

        entry += specInfo[i].mapEntryCount;
      }

      stageCount++;
    }
  }

  static VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

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

  RDCASSERT(ARRAY_COUNT(viattr) >= pipeInfo.vertexAttrs.size());
  RDCASSERT(ARRAY_COUNT(vibind) >= pipeInfo.vertexBindings.size());

  static VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

  ia.topology = pipeInfo.topology;
  ia.primitiveRestartEnable = pipeInfo.primitiveRestartEnable;

  static VkPipelineTessellationStateCreateInfo tess = {
      VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

  tess.patchControlPoints = pipeInfo.patchControlPoints;

  static VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};

  static VkViewport views[32] = {};
  static VkRect2D scissors[32] = {};

  memcpy(views, &pipeInfo.viewports[0], pipeInfo.viewports.size() * sizeof(VkViewport));

  vp.pViewports = &views[0];
  vp.viewportCount = (uint32_t)pipeInfo.viewports.size();

  memcpy(scissors, &pipeInfo.scissors[0], pipeInfo.scissors.size() * sizeof(VkRect2D));

  vp.pScissors = &scissors[0];
  vp.scissorCount = (uint32_t)pipeInfo.scissors.size();

  RDCASSERT(ARRAY_COUNT(views) >= pipeInfo.viewports.size());
  RDCASSERT(ARRAY_COUNT(scissors) >= pipeInfo.scissors.size());

  static VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

  rs.depthClampEnable = pipeInfo.depthClampEnable;
  rs.rasterizerDiscardEnable = pipeInfo.rasterizerDiscardEnable,
  rs.polygonMode = pipeInfo.polygonMode;
  rs.cullMode = pipeInfo.cullMode;
  rs.frontFace = pipeInfo.frontFace;
  rs.depthBiasEnable = pipeInfo.depthBiasEnable;
  rs.depthBiasConstantFactor = pipeInfo.depthBiasConstantFactor;
  rs.depthBiasClamp = pipeInfo.depthBiasClamp;
  rs.depthBiasSlopeFactor = pipeInfo.depthBiasSlopeFactor;
  rs.lineWidth = pipeInfo.lineWidth;

  static VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

  msaa.rasterizationSamples = pipeInfo.rasterizationSamples;
  msaa.sampleShadingEnable = pipeInfo.sampleShadingEnable;
  msaa.minSampleShading = pipeInfo.minSampleShading;
  msaa.pSampleMask = &pipeInfo.sampleMask;
  msaa.alphaToCoverageEnable = pipeInfo.alphaToCoverageEnable;
  msaa.alphaToOneEnable = pipeInfo.alphaToOneEnable;

  static VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

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

  static VkDynamicState dynSt[VK_DYNAMIC_STATE_RANGE_SIZE];

  static VkPipelineDynamicStateCreateInfo dyn = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

  dyn.dynamicStateCount = 0;
  dyn.pDynamicStates = dynSt;

  for(uint32_t i = 0; i < VK_DYNAMIC_STATE_RANGE_SIZE; i++)
    if(pipeInfo.dynamicStates[i])
      dynSt[dyn.dynamicStateCount++] = (VkDynamicState)i;

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
      GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeInfo.layout),
      GetResourceManager()->GetCurrentHandle<VkRenderPass>(pipeInfo.renderpass),
      pipeInfo.subpass,
      VK_NULL_HANDLE,    // base pipeline handle
      0,                 // base pipeline index
  };

  pipeCreateInfo = ret;
}

void VulkanDebugManager::MakeComputePipelineInfo(VkComputePipelineCreateInfo &pipeCreateInfo,
                                                 ResourceId pipeline)
{
  const VulkanCreationInfo::Pipeline &pipeInfo = m_pDriver->m_CreationInfo.m_Pipeline[pipeline];

  VkPipelineShaderStageCreateInfo stage;    // Returned by value
  static VkSpecializationInfo specInfo;
  static vector<VkSpecializationMapEntry> specMapEntries;

  const uint32_t i = 5;    // Compute stage
  RDCASSERT(pipeInfo.shaders[i].module != ResourceId());

  size_t specEntries = 0;
  if(!pipeInfo.shaders[i].specialization.empty())
    specEntries += pipeInfo.shaders[i].specialization.size();

  specMapEntries.resize(specEntries);
  VkSpecializationMapEntry *entry = &specMapEntries[0];

  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = (VkShaderStageFlagBits)(1 << i);
  stage.module = GetResourceManager()->GetCurrentHandle<VkShaderModule>(pipeInfo.shaders[i].module);
  stage.pName = pipeInfo.shaders[i].entryPoint.c_str();
  stage.pNext = NULL;
  stage.pSpecializationInfo = NULL;
  stage.flags = VK_SHADER_STAGE_COMPUTE_BIT;

  if(!pipeInfo.shaders[i].specialization.empty())
  {
    stage.pSpecializationInfo = &specInfo;
    specInfo.pMapEntries = entry;
    specInfo.mapEntryCount = (uint32_t)pipeInfo.shaders[i].specialization.size();

    byte *minDataPtr = NULL;
    byte *maxDataPtr = NULL;

    for(size_t s = 0; s < pipeInfo.shaders[i].specialization.size(); s++)
    {
      entry[s].constantID = pipeInfo.shaders[i].specialization[s].specID;
      entry[s].size = pipeInfo.shaders[i].specialization[s].size;

      if(minDataPtr == NULL)
        minDataPtr = pipeInfo.shaders[i].specialization[s].data;
      else
        minDataPtr = RDCMIN(minDataPtr, pipeInfo.shaders[i].specialization[s].data);

      maxDataPtr = RDCMAX(minDataPtr, pipeInfo.shaders[i].specialization[s].data + entry[s].size);
    }

    for(size_t s = 0; s < pipeInfo.shaders[i].specialization.size(); s++)
      entry[s].offset = (uint32_t)(pipeInfo.shaders[i].specialization[s].data - minDataPtr);

    specInfo.dataSize = (maxDataPtr - minDataPtr);
    specInfo.pData = (const void *)minDataPtr;

    entry += specInfo.mapEntryCount;
  }

  VkComputePipelineCreateInfo ret = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      NULL,
      pipeInfo.flags,
      stage,
      GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeInfo.layout),
      VK_NULL_HANDLE,    // base pipeline handle
      0,                 // base pipeline index
  };

  pipeCreateInfo = ret;
}

void VulkanDebugManager::PatchFixedColShader(VkShaderModule &mod, float col[4])
{
  union
  {
    uint32_t *spirv;
    float *data;
  } alias;

  vector<uint32_t> spv = *m_FixedColSPIRV;

  alias.spirv = &spv[0];
  size_t spirvLength = spv.size();

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = alias.spirv[it] >> spv::WordCountShift;
    spv::Op opcode = spv::Op(alias.spirv[it] & spv::OpCodeMask);

    if(opcode == spv::OpConstant)
    {
      if(alias.data[it + 3] == 1.1f)
        alias.data[it + 3] = col[0];
      else if(alias.data[it + 3] == 2.2f)
        alias.data[it + 3] = col[1];
      else if(alias.data[it + 3] == 3.3f)
        alias.data[it + 3] = col[2];
      else if(alias.data[it + 3] == 4.4f)
        alias.data[it + 3] = col[3];
      else
        RDCERR("Unexpected constant value");
    }

    it += WordCount;
  }

  VkShaderModuleCreateInfo modinfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      NULL,
      0,
      spv.size() * sizeof(uint32_t),
      alias.spirv,
  };

  VkResult vkr = m_pDriver->vkCreateShaderModule(m_Device, &modinfo, NULL, &mod);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

struct VulkanQuadOverdrawCallback : public VulkanDrawcallCallback
{
  VulkanQuadOverdrawCallback(WrappedVulkan *vk, const vector<uint32_t> &events)
      : m_pDriver(vk), m_pDebug(vk->GetDebugManager()), m_Events(events), m_PrevState(vk, NULL)
  {
    m_pDriver->SetDrawcallCB(this);
  }
  ~VulkanQuadOverdrawCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return;

    // we customise the pipeline to disable framebuffer writes, but perform normal testing
    // and substitute our quad calculation fragment shader that writes to a storage image
    // that is bound in a new descriptor set.

    VkResult vkr = VK_SUCCESS;

    m_PrevState = m_pDriver->GetRenderState();
    VulkanRenderState &pipestate = m_pDriver->GetRenderState();

    // check cache first
    pair<uint32_t, VkPipeline> pipe = m_PipelineCache[pipestate.graphics.pipeline];

    // if we don't get a hit, create a modified pipeline
    if(pipe.second == VK_NULL_HANDLE)
    {
      VulkanCreationInfo &c = *pipestate.m_CreationInfo;

      VulkanCreationInfo::Pipeline &p = c.m_Pipeline[pipestate.graphics.pipeline];

      VkDescriptorSetLayout *descSetLayouts;

      // descSet will be the index of our new descriptor set
      uint32_t descSet = (uint32_t)c.m_PipelineLayout[p.layout].descSetLayouts.size();

      descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

      for(uint32_t i = 0; i < descSet; i++)
        descSetLayouts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
            c.m_PipelineLayout[p.layout].descSetLayouts[i]);

      // this layout has storage image and
      descSetLayouts[descSet] = m_pDebug->m_QuadDescSetLayout;

      const vector<VkPushConstantRange> &push = c.m_PipelineLayout[p.layout].pushRanges;

      VkPipelineLayoutCreateInfo pipeLayoutInfo = {
          VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          NULL,
          0,
          descSet + 1,
          descSetLayouts,
          (uint32_t)push.size(),
          push.empty() ? NULL : &push[0],
      };

      // create pipeline layout with same descriptor set layouts, plus our mesh output set
      VkPipelineLayout pipeLayout;
      vkr =
          m_pDriver->vkCreatePipelineLayout(m_pDriver->GetDev(), &pipeLayoutInfo, NULL, &pipeLayout);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SAFE_DELETE_ARRAY(descSetLayouts);

      VkGraphicsPipelineCreateInfo pipeCreateInfo;
      m_pDebug->MakeGraphicsPipelineInfo(pipeCreateInfo, pipestate.graphics.pipeline);

      // repoint pipeline layout
      pipeCreateInfo.layout = pipeLayout;

      // disable colour writes/blends
      VkPipelineColorBlendStateCreateInfo *cb =
          (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
      for(uint32_t i = 0; i < cb->attachmentCount; i++)
      {
        VkPipelineColorBlendAttachmentState *att =
            (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
        att->blendEnable = false;
        att->colorWriteMask = 0x0;
      }

      // disable depth/stencil writes
      VkPipelineDepthStencilStateCreateInfo *ds =
          (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
      ds->depthWriteEnable = false;
      ds->stencilTestEnable = false;
      ds->depthBoundsTestEnable = false;
      ds->front.compareOp = ds->back.compareOp = VK_COMPARE_OP_ALWAYS;
      ds->front.compareMask = ds->back.compareMask = ds->front.writeMask = ds->back.writeMask = 0xff;
      ds->front.reference = ds->back.reference = 0;
      ds->front.passOp = ds->front.failOp = ds->front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds->back.passOp = ds->back.failOp = ds->back.depthFailOp = VK_STENCIL_OP_KEEP;

      // don't discard
      VkPipelineRasterizationStateCreateInfo *rs =
          (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
      rs->rasterizerDiscardEnable = false;

      vector<uint32_t> spirv = *m_pDebug->m_QuadSPIRV;

      // patch spirv, change descriptor set to descSet value
      size_t it = 5;
      while(it < spirv.size())
      {
        uint16_t WordCount = spirv[it] >> spv::WordCountShift;
        spv::Op opcode = spv::Op(spirv[it] & spv::OpCodeMask);

        if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationDescriptorSet)
        {
          spirv[it + 3] = descSet;
          break;
        }

        it += WordCount;
      }

      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          NULL,
          0,
          spirv.size() * sizeof(uint32_t),
          &spirv[0],
      };

      VkShaderModule module;

      VkDevice dev = m_pDriver->GetDev();

      vkr = ObjDisp(dev)->CreateShaderModule(Unwrap(dev), &modinfo, NULL, &module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), module);

      bool found = false;
      for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
      {
        VkPipelineShaderStageCreateInfo &sh =
            (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
        if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
          sh.module = module;
          sh.pName = "main";
          found = true;
          break;
        }
      }

      if(!found)
      {
        // we know this is safe because it's pointing to a static array that's
        // big enough for all shaders

        VkPipelineShaderStageCreateInfo &sh =
            (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
        sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        sh.pNext = NULL;
        sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        sh.module = module;
        sh.pName = "main";
        sh.pSpecializationInfo = NULL;
      }

      vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                                 &pipe.second);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      ObjDisp(dev)->DestroyShaderModule(Unwrap(dev), Unwrap(module), NULL);
      m_pDriver->GetResourceManager()->ReleaseWrappedResource(module);

      pipe.first = descSet;

      m_PipelineCache[pipestate.graphics.pipeline] = pipe;
    }

    // modify state for first draw call
    pipestate.graphics.pipeline = GetResID(pipe.second);
    RDCASSERT(pipestate.graphics.descSets.size() >= pipe.first);
    pipestate.graphics.descSets.resize(pipe.first + 1);
    pipestate.graphics.descSets[pipe.first].descSet = GetResID(m_pDebug->m_QuadDescSet);

    if(cmd)
      pipestate.BindPipeline(cmd, VulkanRenderState::BindGraphics, false);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return false;

    // restore the render state and go ahead with the real draw
    m_pDriver->GetRenderState() = m_PrevState;

    RDCASSERT(cmd);
    m_pDriver->GetRenderState().BindPipeline(cmd, VulkanRenderState::BindGraphics, false);

    return true;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool RecordAllCmds() { return false; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // don't care
  }

  WrappedVulkan *m_pDriver;
  VulkanDebugManager *m_pDebug;
  const vector<uint32_t> &m_Events;

  // cache modified pipelines
  map<ResourceId, pair<uint32_t, VkPipeline> > m_PipelineCache;
  VulkanRenderState m_PrevState;
};

ResourceId VulkanDebugManager::RenderOverlay(ResourceId texid, DebugOverlay overlay,
                                             uint32_t eventID, const vector<uint32_t> &passEvents)
{
  const VkLayerDispatchTable *vt = ObjDisp(m_Device);

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // if the overlay image is the wrong size, free it
  if(m_OverlayImage != VK_NULL_HANDLE &&
     (iminfo.extent.width != m_OverlayDim.width || iminfo.extent.height != m_OverlayDim.height))
  {
    m_pDriver->vkDestroyRenderPass(m_Device, m_OverlayNoDepthRP, NULL);
    m_pDriver->vkDestroyFramebuffer(m_Device, m_OverlayNoDepthFB, NULL);
    m_pDriver->vkDestroyImageView(m_Device, m_OverlayImageView, NULL);
    m_pDriver->vkDestroyImage(m_Device, m_OverlayImage, NULL);

    m_OverlayImage = VK_NULL_HANDLE;
    m_OverlayImageView = VK_NULL_HANDLE;
    m_OverlayNoDepthRP = VK_NULL_HANDLE;
    m_OverlayNoDepthFB = VK_NULL_HANDLE;
  }

  // create the overlay image if we don't have one already
  // we go through the driver's creation functions so creation info
  // is saved and the resources are registered as live resources for
  // their IDs.
  if(m_OverlayImage == VK_NULL_HANDLE)
  {
    m_OverlayDim.width = iminfo.extent.width;
    m_OverlayDim.height = iminfo.extent.height;

    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        {m_OverlayDim.width, m_OverlayDim.height, 1},
        1,
        1,
        iminfo.samples,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, NULL, &m_OverlayImage);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetImageMemoryRequirements(m_Device, m_OverlayImage, &mrq);

    // if no memory is allocated, or it's not enough,
    // then allocate
    if(m_OverlayImageMem == VK_NULL_HANDLE || mrq.size > m_OverlayMemSize)
    {
      if(m_OverlayImageMem != VK_NULL_HANDLE)
      {
        m_pDriver->vkFreeMemory(m_Device, m_OverlayImageMem, NULL);
      }

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &m_OverlayImageMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_OverlayMemSize = mrq.size;
    }

    vkr = m_pDriver->vkBindImageMemory(m_Device, m_OverlayImage, m_OverlayImageMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        m_OverlayImage,
        VK_IMAGE_VIEW_TYPE_2D,
        imInfo.format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
        },
    };

    vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &m_OverlayImageView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // need to update image layout into valid state

    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
        Unwrap(m_OverlayImage),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    m_pDriver->m_ImageLayouts[GetResID(m_OverlayImage)].subresourceStates[0].newLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    DoPipelineBarrier(cmd, 1, &barrier);

    VkAttachmentDescription colDesc = {0,
                                       imInfo.format,
                                       imInfo.samples,
                                       VK_ATTACHMENT_LOAD_OP_LOAD,
                                       VK_ATTACHMENT_STORE_OP_STORE,
                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                       VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub = {
        0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,    NULL,       // inputs
        1,    &colRef,    // color
        NULL,             // resolve
        NULL,             // depth-stencil
        0,    NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        1,
        &colDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &m_OverlayNoDepthRP);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // Create framebuffer rendering just to overlay image, no depth
    VkFramebufferCreateInfo fbinfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        m_OverlayNoDepthRP,
        1,
        &m_OverlayImageView,
        (uint32_t)m_OverlayDim.width,
        (uint32_t)m_OverlayDim.height,
        1,
    };

    vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_OverlayNoDepthFB);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // can't create a framebuffer or renderpass for overlay image + depth as that
    // needs to match the depth texture type wherever our draw is.
  }

  VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  const DrawcallDescription *mainDraw = m_pDriver->GetDrawcall(eventID);

  // Secondary commands can't have render passes
  if((mainDraw && !(mainDraw->flags & DrawFlags::Drawcall)) ||
     !m_pDriver->m_Partial[WrappedVulkan::Primary].renderPassActive)
  {
    // don't do anything, no drawcall capable of making overlays selected
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);
  }
  else if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);
  }
  else if(overlay == DebugOverlay::Drawcall || overlay == DebugOverlay::Wireframe)
  {
    float highlightCol[] = {0.8f, 0.1f, 0.8f, 0.0f};

    if(overlay == DebugOverlay::Wireframe)
    {
      highlightCol[0] = 200 / 255.0f;
      highlightCol[1] = 1.0f;
      highlightCol[2] = 0.0f;
    }

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)highlightCol, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    highlightCol[3] = 1.0f;

    // backup state
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    // make patched shader
    VkShaderModule mod = VK_NULL_HANDLE;

    PatchFixedColShader(mod, highlightCol);

    // make patched pipeline
    VkGraphicsPipelineCreateInfo pipeCreateInfo;

    MakeGraphicsPipelineInfo(pipeCreateInfo, prevstate.graphics.pipeline);

    // disable all tests possible
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    ds->depthTestEnable = false;
    ds->depthWriteEnable = false;
    ds->stencilTestEnable = false;
    ds->depthBoundsTestEnable = false;

    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    rs->cullMode = VK_CULL_MODE_NONE;
    rs->rasterizerDiscardEnable = false;

    if(m_pDriver->GetDeviceFeatures().depthClamp)
    {
      rs->depthClampEnable = true;
    }

    if(overlay == DebugOverlay::Wireframe && m_pDriver->GetDeviceFeatures().fillModeNonSolid)
    {
      rs->polygonMode = VK_POLYGON_MODE_LINE;
      rs->lineWidth = 1.0f;
    }

    VkPipelineColorBlendStateCreateInfo *cb =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    cb->logicOpEnable = false;
    cb->attachmentCount = 1;    // only one colour attachment
    for(uint32_t i = 0; i < cb->attachmentCount; i++)
    {
      VkPipelineColorBlendAttachmentState *att =
          (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
      att->blendEnable = false;
      att->colorWriteMask = 0xf;
    }

    // set scissors to max
    for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
    {
      VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
      sc.offset.x = 0;
      sc.offset.y = 0;
      sc.extent.width = 16384;
      sc.extent.height = 16384;
    }

    // set our renderpass and shader
    pipeCreateInfo.renderPass = m_OverlayNoDepthRP;
    pipeCreateInfo.subpass = 0;

    bool found = false;
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
      if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        sh.module = mod;
        sh.pName = "main";
        found = true;
        break;
      }
    }

    if(!found)
    {
      // we know this is safe because it's pointing to a static array that's
      // big enough for all shaders

      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
      sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.pNext = NULL;
      sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      sh.module = mod;
      sh.pName = "main";
      sh.pSpecializationInfo = NULL;
    }

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkPipeline pipe = VK_NULL_HANDLE;

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // modify state
    m_pDriver->m_RenderState.renderPass = GetResID(m_OverlayNoDepthRP);
    m_pDriver->m_RenderState.subpass = 0;
    m_pDriver->m_RenderState.framebuffer = GetResID(m_OverlayNoDepthFB);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe);

    // set dynamic scissors in case pipeline was using them
    for(size_t i = 0; i < m_pDriver->m_RenderState.scissors.size(); i++)
    {
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].extent.width = 16384;
      m_pDriver->m_RenderState.scissors[i].extent.height = 16384;
    }

    if(overlay == DebugOverlay::Wireframe)
      m_pDriver->m_RenderState.lineWidth = 1.0f;

    m_pDriver->ReplayLog(0, eventID, eReplay_OnlyDraw);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    m_pDriver->vkDestroyPipeline(m_Device, pipe, NULL);
    m_pDriver->vkDestroyShaderModule(m_Device, mod, NULL);
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    // clear the whole image to opaque black. We'll overwite the render area with transparent black
    // before rendering the viewport/scissors
    float black[] = {0.0f, 0.0f, 0.0f, 1.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    black[3] = 0.0f;

    {
      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          Unwrap(m_OverlayNoDepthRP),
          Unwrap(m_OverlayNoDepthFB),
          m_pDriver->m_RenderState.renderArea,
          1,
          &clearval,
      };
      vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      VkClearRect rect = {
          {{
               m_pDriver->m_RenderState.renderArea.offset.x,
               m_pDriver->m_RenderState.renderArea.offset.y,
           },
           {
               m_pDriver->m_RenderState.renderArea.extent.width,
               m_pDriver->m_RenderState.renderArea.extent.height,
           }},
          0,
          1,
      };
      VkClearAttachment blackclear = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
      vt->CmdClearAttachments(Unwrap(cmd), 1, &blackclear, 1, &rect);

      VkViewport viewport = m_pDriver->m_RenderState.views[0];
      vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

      uint32_t uboOffs = 0;

      OutlineUBOData *ubo = (OutlineUBOData *)m_OutlineUBO.Map(&uboOffs);

      ubo->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
      ubo->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
      ubo->Scissor = 0;
      ubo->ViewRect = Vec4f(viewport.x, viewport.y, viewport.width, viewport.height);

      if(m_pDriver->m_ExtensionsEnabled[VkCheckExt_AMD_neg_viewport])
        ubo->ViewRect.w = fabs(viewport.height);

      m_OutlineUBO.Unmap();

      vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          Unwrap(m_OutlinePipeline[SampleIndex(iminfo.samples)]));
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_OutlinePipeLayout), 0, 1, UnwrapPtr(m_OutlineDescSet), 1,
                                &uboOffs);

      vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

      if(!m_pDriver->m_RenderState.scissors.empty())
      {
        Vec4f scissor((float)m_pDriver->m_RenderState.scissors[0].offset.x,
                      (float)m_pDriver->m_RenderState.scissors[0].offset.y,
                      (float)m_pDriver->m_RenderState.scissors[0].extent.width,
                      (float)m_pDriver->m_RenderState.scissors[0].extent.height);

        ubo = (OutlineUBOData *)m_OutlineUBO.Map(&uboOffs);

        ubo->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
        ubo->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
        ubo->Scissor = 1;
        ubo->ViewRect = scissor;

        m_OutlineUBO.Unmap();

        viewport.x = scissor.x;
        viewport.y = scissor.y;
        viewport.width = scissor.z;
        viewport.height = scissor.w;

        vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
        vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(m_OutlinePipeLayout), 0, 1, UnwrapPtr(m_OutlineDescSet), 1,
                                  &uboOffs);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }

      vt->CmdEndRenderPass(Unwrap(cmd));
    }
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    float highlightCol[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)highlightCol, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    highlightCol[0] = 1.0f;
    highlightCol[3] = 1.0f;

    // backup state
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    // make patched shader
    VkShaderModule mod[2] = {0};
    VkPipeline pipe[2] = {0};

    // first shader, no culling, writes red
    PatchFixedColShader(mod[0], highlightCol);

    highlightCol[0] = 0.0f;
    highlightCol[1] = 1.0f;

    // second shader, normal culling, writes green
    PatchFixedColShader(mod[1], highlightCol);

    // make patched pipeline
    VkGraphicsPipelineCreateInfo pipeCreateInfo;

    MakeGraphicsPipelineInfo(pipeCreateInfo, prevstate.graphics.pipeline);

    // disable all tests possible
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    ds->depthTestEnable = false;
    ds->depthWriteEnable = false;
    ds->stencilTestEnable = false;
    ds->depthBoundsTestEnable = false;

    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    VkCullModeFlags origCullMode = rs->cullMode;
    rs->cullMode = VK_CULL_MODE_NONE;    // first render without any culling
    rs->rasterizerDiscardEnable = false;

    if(m_pDriver->GetDeviceFeatures().depthClamp)
      rs->depthClampEnable = true;

    VkPipelineColorBlendStateCreateInfo *cb =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    cb->logicOpEnable = false;
    cb->attachmentCount = 1;    // only one colour attachment
    for(uint32_t i = 0; i < cb->attachmentCount; i++)
    {
      VkPipelineColorBlendAttachmentState *att =
          (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
      att->blendEnable = false;
      att->colorWriteMask = 0xf;
    }

    // set scissors to max
    for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
    {
      VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
      sc.offset.x = 0;
      sc.offset.y = 0;
      sc.extent.width = 16384;
      sc.extent.height = 16384;
    }

    // set our renderpass and shader
    pipeCreateInfo.renderPass = m_OverlayNoDepthRP;
    pipeCreateInfo.subpass = 0;

    VkPipelineShaderStageCreateInfo *fragShader = NULL;

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
      if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        sh.module = mod[0];
        sh.pName = "main";
        fragShader = &sh;
        break;
      }
    }

    if(fragShader == NULL)
    {
      // we know this is safe because it's pointing to a static array that's
      // big enough for all shaders

      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
      sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.pNext = NULL;
      sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      sh.module = mod[0];
      sh.pName = "main";
      sh.pSpecializationInfo = NULL;

      fragShader = &sh;
    }

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    fragShader->module = mod[1];
    rs->cullMode = origCullMode;

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe[1]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // modify state
    m_pDriver->m_RenderState.renderPass = GetResID(m_OverlayNoDepthRP);
    m_pDriver->m_RenderState.subpass = 0;
    m_pDriver->m_RenderState.framebuffer = GetResID(m_OverlayNoDepthFB);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe[0]);

    // set dynamic scissors in case pipeline was using them
    for(size_t i = 0; i < m_pDriver->m_RenderState.scissors.size(); i++)
    {
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].extent.width = 16384;
      m_pDriver->m_RenderState.scissors[i].extent.height = 16384;
    }

    m_pDriver->ReplayLog(0, eventID, eReplay_OnlyDraw);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe[1]);

    m_pDriver->ReplayLog(0, eventID, eReplay_OnlyDraw);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    for(int i = 0; i < 2; i++)
    {
      m_pDriver->vkDestroyPipeline(m_Device, pipe[i], NULL);
      m_pDriver->vkDestroyShaderModule(m_Device, mod[i], NULL);
    }
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    float highlightCol[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)highlightCol, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    VkFramebuffer depthFB = VK_NULL_HANDLE;
    VkRenderPass depthRP = VK_NULL_HANDLE;

    const VulkanRenderState &state = m_pDriver->m_RenderState;
    VulkanCreationInfo &createinfo = m_pDriver->m_CreationInfo;

    RDCASSERT(state.subpass < createinfo.m_RenderPass[state.renderPass].subpasses.size());
    int32_t dsIdx =
        createinfo.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;

    // make a renderpass and framebuffer for rendering to overlay color and using
    // depth buffer from the orignial render
    if(dsIdx >= 0 && dsIdx < (int32_t)createinfo.m_Framebuffer[state.framebuffer].attachments.size())
    {
      VkAttachmentDescription attDescs[] = {
          {0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
           VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
           VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          {0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
           VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
           VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
      };

      ResourceId depthView = createinfo.m_Framebuffer[state.framebuffer].attachments[dsIdx].view;
      ResourceId depthIm = createinfo.m_ImageView[depthView].image;

      attDescs[1].format = createinfo.m_Image[depthIm].format;
      attDescs[0].samples = attDescs[1].samples = iminfo.samples;

      VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      VkAttachmentReference dsRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

      VkSubpassDescription sub = {
          0,      VK_PIPELINE_BIND_POINT_GRAPHICS,
          0,      NULL,       // inputs
          1,      &colRef,    // color
          NULL,               // resolve
          &dsRef,             // depth-stencil
          0,      NULL,       // preserve
      };

      VkRenderPassCreateInfo rpinfo = {
          VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
          NULL,
          0,
          2,
          attDescs,
          1,
          &sub,
          0,
          NULL,    // dependencies
      };

      vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &depthRP);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageView views[] = {
          m_OverlayImageView, GetResourceManager()->GetCurrentHandle<VkImageView>(depthView),
      };

      // Create framebuffer rendering just to overlay image, no depth
      VkFramebufferCreateInfo fbinfo = {
          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          NULL,
          0,
          depthRP,
          2,
          views,
          (uint32_t)m_OverlayDim.width,
          (uint32_t)m_OverlayDim.height,
          1,
      };

      vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &depthFB);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    // if depthRP is NULL, so is depthFB, and it means no depth buffer was
    // bound, so we just render green.

    highlightCol[0] = 1.0f;
    highlightCol[3] = 1.0f;

    // backup state
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    // make patched shader
    VkShaderModule mod[2] = {0};
    VkPipeline pipe[2] = {0};

    // first shader, no depth testing, writes red
    PatchFixedColShader(mod[0], highlightCol);

    highlightCol[0] = 0.0f;
    highlightCol[1] = 1.0f;

    // second shader, enabled depth testing, writes green
    PatchFixedColShader(mod[1], highlightCol);

    // make patched pipeline
    VkGraphicsPipelineCreateInfo pipeCreateInfo;

    MakeGraphicsPipelineInfo(pipeCreateInfo, prevstate.graphics.pipeline);

    // disable all tests possible
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    VkBool32 origDepthTest = ds->depthTestEnable;
    ds->depthTestEnable = false;
    ds->depthWriteEnable = false;
    VkBool32 origStencilTest = ds->stencilTestEnable;
    ds->stencilTestEnable = false;
    ds->depthBoundsTestEnable = false;

    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    rs->cullMode = VK_CULL_MODE_NONE;
    rs->rasterizerDiscardEnable = false;

    if(m_pDriver->GetDeviceFeatures().depthClamp)
      rs->depthClampEnable = true;

    VkPipelineColorBlendStateCreateInfo *cb =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    cb->logicOpEnable = false;
    cb->attachmentCount = 1;    // only one colour attachment
    for(uint32_t i = 0; i < cb->attachmentCount; i++)
    {
      VkPipelineColorBlendAttachmentState *att =
          (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
      att->blendEnable = false;
      att->colorWriteMask = 0xf;
    }

    // set scissors to max
    for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
    {
      VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
      sc.offset.x = 0;
      sc.offset.y = 0;
      sc.extent.width = 16384;
      sc.extent.height = 16384;
    }

    // set our renderpass and shader
    pipeCreateInfo.renderPass = m_OverlayNoDepthRP;
    pipeCreateInfo.subpass = 0;

    VkPipelineShaderStageCreateInfo *fragShader = NULL;

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
      if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        sh.module = mod[0];
        sh.pName = "main";
        fragShader = &sh;
        break;
      }
    }

    if(fragShader == NULL)
    {
      // we know this is safe because it's pointing to a static array that's
      // big enough for all shaders

      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
      sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.pNext = NULL;
      sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      sh.module = mod[0];
      sh.pName = "main";
      sh.pSpecializationInfo = NULL;

      fragShader = &sh;
    }

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    fragShader->module = mod[1];

    if(depthRP != VK_NULL_HANDLE)
    {
      if(overlay == DebugOverlay::Depth)
        ds->depthTestEnable = origDepthTest;
      else
        ds->stencilTestEnable = origStencilTest;
      pipeCreateInfo.renderPass = depthRP;
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe[1]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // modify state
    m_pDriver->m_RenderState.renderPass = GetResID(m_OverlayNoDepthRP);
    m_pDriver->m_RenderState.subpass = 0;
    m_pDriver->m_RenderState.framebuffer = GetResID(m_OverlayNoDepthFB);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe[0]);

    // set dynamic scissors in case pipeline was using them
    for(size_t i = 0; i < m_pDriver->m_RenderState.scissors.size(); i++)
    {
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].extent.width = 16384;
      m_pDriver->m_RenderState.scissors[i].extent.height = 16384;
    }

    m_pDriver->ReplayLog(0, eventID, eReplay_OnlyDraw);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe[1]);
    if(depthRP != VK_NULL_HANDLE)
    {
      m_pDriver->m_RenderState.renderPass = GetResID(depthRP);
      m_pDriver->m_RenderState.framebuffer = GetResID(depthFB);
    }

    m_pDriver->ReplayLog(0, eventID, eReplay_OnlyDraw);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    for(int i = 0; i < 2; i++)
    {
      m_pDriver->vkDestroyPipeline(m_Device, pipe[i], NULL);
      m_pDriver->vkDestroyShaderModule(m_Device, mod[i], NULL);
    }

    if(depthRP != VK_NULL_HANDLE)
    {
      m_pDriver->vkDestroyRenderPass(m_Device, depthRP, NULL);
      m_pDriver->vkDestroyFramebuffer(m_Device, depthFB, NULL);
    }
  }
  else if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
  {
    // clear the overlay image itself
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_OverlayImage),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventID);

    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif

      size_t startEvent = 0;

      // if we're ClearBeforePass the first event will be a vkBeginRenderPass.
      // if there are any other events, we need to play up to right before them
      // so that we have all the render state set up to do
      // BeginRenderPassAndApplyState and a clear. If it's just the begin, we
      // just play including it, do the clear, then we won't replay anything
      // in the loop below
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[0]);
        if(draw && draw->flags & DrawFlags::BeginPass)
        {
          if(events.size() == 1)
          {
            m_pDriver->ReplayLog(0, events[0], eReplay_Full);
          }
          else
          {
            startEvent = 1;
            m_pDriver->ReplayLog(0, events[1], eReplay_WithoutDraw);
          }
        }
      }
      else
      {
        m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->m_RenderState.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

      VkClearAttachment blackclear = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
      vector<VkClearAttachment> atts;

      VulkanCreationInfo::Framebuffer &fb =
          m_pDriver->m_CreationInfo.m_Framebuffer[m_pDriver->m_RenderState.framebuffer];
      VulkanCreationInfo::RenderPass &rp =
          m_pDriver->m_CreationInfo.m_RenderPass[m_pDriver->m_RenderState.renderPass];

      for(size_t i = 0; i < rp.subpasses[m_pDriver->m_RenderState.subpass].colorAttachments.size();
          i++)
      {
        blackclear.colorAttachment =
            rp.subpasses[m_pDriver->m_RenderState.subpass].colorAttachments[i];
        atts.push_back(blackclear);
      }

      VkClearRect rect = {
          {
              {0, 0}, {fb.width, fb.height},
          },
          0,
          1,
      };

      vt->CmdClearAttachments(Unwrap(cmd), (uint32_t)atts.size(), &atts[0], 1, &rect);

      m_pDriver->m_RenderState.EndRenderPass(cmd);

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      for(size_t i = startEvent; i < events.size(); i++)
      {
        m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDriver->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    {
      SCOPED_TIMER("Quad Overdraw");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

      VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      NULL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      Unwrap(m_OverlayImage),
                                      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      DoPipelineBarrier(cmd, 1, &barrier);

      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1,
                             &subresourceRange);

      std::swap(barrier.oldLayout, barrier.newLayout);
      std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
      barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

      DoPipelineBarrier(cmd, 1, &barrier);

      vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventID);

      VkImage quadImg;
      VkDeviceMemory quadImgMem;
      VkImageView quadImgView;

      VkImageCreateInfo imInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          NULL,
          0,
          VK_IMAGE_TYPE_2D,
          VK_FORMAT_R32_UINT,
          {RDCMAX(1U, m_OverlayDim.width >> 1), RDCMAX(1U, m_OverlayDim.height >> 1), 1},
          1,
          4,
          VK_SAMPLE_COUNT_1_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_SHARING_MODE_EXCLUSIVE,
          0,
          NULL,
          VK_IMAGE_LAYOUT_UNDEFINED,
      };

      vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, NULL, &quadImg);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkMemoryRequirements mrq = {0};

      m_pDriver->vkGetImageMemoryRequirements(m_Device, quadImg, &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &quadImgMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      vkr = m_pDriver->vkBindImageMemory(m_Device, quadImg, quadImgMem, 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageViewCreateInfo viewinfo = {
          VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          NULL,
          0,
          quadImg,
          VK_IMAGE_VIEW_TYPE_2D_ARRAY,
          VK_FORMAT_R32_UINT,
          {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
           VK_COMPONENT_SWIZZLE_ONE},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 4},
      };

      vkr = m_pDriver->vkCreateImageView(m_Device, &viewinfo, NULL, &quadImgView);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      // update descriptor to point to our R32 result image
      VkDescriptorImageInfo imdesc = {0};
      imdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      imdesc.sampler = VK_NULL_HANDLE;
      imdesc.imageView = Unwrap(quadImgView);

      VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    NULL,
                                    Unwrap(m_QuadDescSet),
                                    0,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                    &imdesc,
                                    NULL,
                                    NULL};
      vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

      VkImageMemoryBarrier quadImBarrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          0,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_GENERAL,
          0,
          0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
          Unwrap(quadImg),
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 4}};

      // clear all to black
      DoPipelineBarrier(cmd, 1, &quadImBarrier);
      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(quadImg), VK_IMAGE_LAYOUT_GENERAL,
                             (VkClearColorValue *)&black, 1, &quadImBarrier.subresourceRange);

      quadImBarrier.srcAccessMask = quadImBarrier.dstAccessMask;
      quadImBarrier.oldLayout = quadImBarrier.newLayout;

      quadImBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

      // set to general layout, for load/store operations
      DoPipelineBarrier(cmd, 1, &quadImBarrier);

      VkMemoryBarrier memBarrier = {
          VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_ALL_WRITE_BITS, VK_ACCESS_ALL_READ_BITS,
      };

      DoPipelineBarrier(cmd, 1, &memBarrier);

      // end this cmd buffer so the image is in the right state for the next part
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif

      m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);

      // declare callback struct here
      VulkanQuadOverdrawCallback cb(m_pDriver, events);

      m_pDriver->ReplayLog(events.front(), events.back(), eReplay_Full);

      // resolve pass
      {
        cmd = m_pDriver->GetNextCmd();

        vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        quadImBarrier.srcAccessMask = quadImBarrier.dstAccessMask;
        quadImBarrier.oldLayout = quadImBarrier.newLayout;

        quadImBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // wait for writing to finish
        DoPipelineBarrier(cmd, 1, &quadImBarrier);

        VkClearValue clearval = {};
        VkRenderPassBeginInfo rpbegin = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            NULL,
            Unwrap(m_OverlayNoDepthRP),
            Unwrap(m_OverlayNoDepthFB),
            m_pDriver->m_RenderState.renderArea,
            1,
            &clearval,
        };
        vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

        vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Unwrap(m_QuadResolvePipeline[SampleIndex(iminfo.samples)]));
        vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(m_QuadResolvePipeLayout), 0, 1, UnwrapPtr(m_QuadDescSet),
                                  0, NULL);

        VkViewport viewport = {0.0f, 0.0f, (float)m_OverlayDim.width, (float)m_OverlayDim.height,
                               0.0f, 1.0f};
        vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
        vt->CmdEndRenderPass(Unwrap(cmd));

        vkr = vt->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->vkDestroyImageView(m_Device, quadImgView, NULL);
      m_pDriver->vkDestroyImage(m_Device, quadImg, NULL);
      m_pDriver->vkFreeMemory(m_Device, quadImgMem, NULL);

      for(auto it = cb.m_PipelineCache.begin(); it != cb.m_PipelineCache.end(); ++it)
      {
        m_pDriver->vkDestroyPipeline(m_Device, it->second.second, NULL);
      }
    }

    // restore back to normal
    m_pDriver->ReplayLog(0, eventID, eReplay_WithoutDraw);

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else if(overlay == DebugOverlay::TriangleSizePass || overlay == DebugOverlay::TriangleSizeDraw)
  {
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    {
      SCOPED_TIMER("Triangle Size");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

      VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      NULL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      Unwrap(m_OverlayImage),
                                      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      DoPipelineBarrier(cmd, 1, &barrier);

      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1,
                             &subresourceRange);

      std::swap(barrier.oldLayout, barrier.newLayout);
      std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
      barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

      DoPipelineBarrier(cmd, 1, &barrier);

      // end this cmd buffer so the image is in the right state for the next part
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif

      vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::TriangleSizeDraw)
        events.clear();

      while(!events.empty())
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[0]);

        // remove any non-drawcalls, like the pass boundary.
        if(!(draw->flags & DrawFlags::Drawcall))
          events.erase(events.begin());
        else
          break;
      }

      events.push_back(eventID);

      m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);

      VulkanRenderState &state = m_pDriver->GetRenderState();

      uint32_t meshOffs = 0;
      MeshUBOData *data = (MeshUBOData *)m_MeshUBO.Map(&meshOffs);

      data->mvp = Matrix4f::Identity();
      data->invProj = Matrix4f::Identity();
      data->color = Vec4f();
      data->homogenousInput = 1;
      data->pointSpriteSize = Vec2f(0.0f, 0.0f);
      data->displayFormat = 0;
      data->rawoutput = 1;
      data->padding = Vec3f();
      m_MeshUBO.Unmap();

      uint32_t viewOffs = 0;
      Vec4f *ubo = (Vec4f *)m_TriSizeUBO.Map(&viewOffs);
      *ubo = Vec4f(state.views[0].width, state.views[0].height);
      m_TriSizeUBO.Unmap();

      uint32_t offsets[2] = {meshOffs, viewOffs};

      VkDescriptorBufferInfo bufdesc;
      m_MeshUBO.FillDescriptor(bufdesc);

      VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    NULL,
                                    Unwrap(m_TriSizeDescSet),
                                    0,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    NULL,
                                    &bufdesc,
                                    NULL};
      vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

      m_TriSizeUBO.FillDescriptor(bufdesc);
      write.dstBinding = 2;
      vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

      VkRenderPass RP = m_OverlayNoDepthRP;
      VkFramebuffer FB = m_OverlayNoDepthFB;

      VulkanCreationInfo &createinfo = m_pDriver->m_CreationInfo;

      RDCASSERT(state.subpass < createinfo.m_RenderPass[state.renderPass].subpasses.size());
      int32_t dsIdx =
          createinfo.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;

      bool depthUsed = false;

      // make a renderpass and framebuffer for rendering to overlay color and using
      // depth buffer from the orignial render
      if(dsIdx >= 0 && dsIdx < (int32_t)createinfo.m_Framebuffer[state.framebuffer].attachments.size())
      {
        depthUsed = true;

        VkAttachmentDescription attDescs[] = {
            {0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
             VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
             VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
             VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
             VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
        };

        ResourceId depthView = createinfo.m_Framebuffer[state.framebuffer].attachments[dsIdx].view;
        ResourceId depthIm = createinfo.m_ImageView[depthView].image;

        attDescs[1].format = createinfo.m_Image[depthIm].format;
        attDescs[0].samples = attDescs[1].samples = iminfo.samples;

        VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference dsRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription sub = {
            0,      VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,      NULL,       // inputs
            1,      &colRef,    // color
            NULL,               // resolve
            &dsRef,             // depth-stencil
            0,      NULL,       // preserve
        };

        VkRenderPassCreateInfo rpinfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            NULL,
            0,
            2,
            attDescs,
            1,
            &sub,
            0,
            NULL,    // dependencies
        };

        vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &RP);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageView views[] = {
            m_OverlayImageView, GetResourceManager()->GetCurrentHandle<VkImageView>(depthView),
        };

        // Create framebuffer rendering just to overlay image, no depth
        VkFramebufferCreateInfo fbinfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            NULL,
            0,
            RP,
            2,
            views,
            (uint32_t)m_OverlayDim.width,
            (uint32_t)m_OverlayDim.height,
            1,
        };

        vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &FB);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      VkGraphicsPipelineCreateInfo pipeCreateInfo;

      MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

      VkPipelineShaderStageCreateInfo stages[3] = {
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
           m_MeshModules[0], "main", NULL},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
           VK_SHADER_STAGE_FRAGMENT_BIT, m_TriSizeFSModule, "main", NULL},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
           VK_SHADER_STAGE_GEOMETRY_BIT, m_TriSizeGSModule, "main", NULL},
      };

      VkPipelineInputAssemblyStateCreateInfo ia = {
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
      ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

      VkVertexInputBindingDescription binds[] = {// primary
                                                 {0, 0, VK_VERTEX_INPUT_RATE_VERTEX},
                                                 // secondary
                                                 {1, 0, VK_VERTEX_INPUT_RATE_VERTEX}};

      VkVertexInputAttributeDescription vertAttrs[] = {
          {
              0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0,
          },
          {
              1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0,
          },
      };

      VkPipelineVertexInputStateCreateInfo vi = {
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          NULL,
          0,
          1,
          binds,
          2,
          vertAttrs,
      };

      VkPipelineColorBlendAttachmentState attState = {
          false,
          VK_BLEND_FACTOR_ONE,
          VK_BLEND_FACTOR_ZERO,
          VK_BLEND_OP_ADD,
          VK_BLEND_FACTOR_ONE,
          VK_BLEND_FACTOR_ZERO,
          VK_BLEND_OP_ADD,
          0xf,
      };

      VkPipelineColorBlendStateCreateInfo cb = {
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          NULL,
          0,
          false,
          VK_LOGIC_OP_NO_OP,
          1,
          &attState,
          {1.0f, 1.0f, 1.0f, 1.0f}};

      pipeCreateInfo.stageCount = 3;
      pipeCreateInfo.pStages = stages;
      pipeCreateInfo.pTessellationState = NULL;
      pipeCreateInfo.renderPass = RP;
      pipeCreateInfo.subpass = 0;
      pipeCreateInfo.layout = m_TriSizePipeLayout;
      pipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
      pipeCreateInfo.basePipelineIndex = 0;
      pipeCreateInfo.pInputAssemblyState = &ia;
      pipeCreateInfo.pVertexInputState = &vi;
      pipeCreateInfo.pColorBlendState = &cb;

      typedef std::pair<uint32_t, Topology> PipeKey;

      map<PipeKey, VkPipeline> pipes;

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          Unwrap(RP),
          Unwrap(FB),
          {{
               0, 0,
           },
           m_OverlayDim},
          1,
          &clearval,
      };
      vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {0.0f, 0.0f, (float)m_OverlayDim.width, (float)m_OverlayDim.height,
                             0.0f, 1.0f};
      vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

      for(size_t i = 0; i < events.size(); i++)
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::GSOut);
          if(fmt.buf == ResourceId())
            fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::VSOut);

          if(fmt.buf != ResourceId())
          {
            ia.topology = MakeVkPrimitiveTopology(fmt.topo);

            binds[0].stride = binds[1].stride = fmt.stride;

            PipeKey key = std::make_pair(fmt.stride, fmt.topo);
            VkPipeline pipe = pipes[key];

            if(pipe == VK_NULL_HANDLE)
            {
              vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
                                                         &pipeCreateInfo, NULL, &pipe);
              RDCASSERTEQUAL(vkr, VK_SUCCESS);
            }

            VkBuffer vb = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.buf);

            VkDeviceSize offs = fmt.offset;
            vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);

            pipes[key] = pipe;

            vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Unwrap(m_TriSizePipeLayout), 0, 1,
                                      UnwrapPtr(m_TriSizeDescSet), 2, offsets);

            vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

            const VkPipelineDynamicStateCreateInfo *dyn = pipeCreateInfo.pDynamicState;

            for(uint32_t dynState = 0; dyn && dynState < dyn->dynamicStateCount; dynState++)
            {
              VkDynamicState d = dyn->pDynamicStates[dynState];

              if(!state.views.empty() && d == VK_DYNAMIC_STATE_VIEWPORT)
              {
                vt->CmdSetViewport(Unwrap(cmd), 0, (uint32_t)state.views.size(), &state.views[0]);
              }
              else if(!state.scissors.empty() && d == VK_DYNAMIC_STATE_SCISSOR)
              {
                vt->CmdSetScissor(Unwrap(cmd), 0, (uint32_t)state.scissors.size(),
                                  &state.scissors[0]);
              }
              else if(d == VK_DYNAMIC_STATE_LINE_WIDTH)
              {
                vt->CmdSetLineWidth(Unwrap(cmd), state.lineWidth);
              }
              else if(d == VK_DYNAMIC_STATE_DEPTH_BIAS)
              {
                vt->CmdSetDepthBias(Unwrap(cmd), state.bias.depth, state.bias.biasclamp,
                                    state.bias.slope);
              }
              else if(d == VK_DYNAMIC_STATE_BLEND_CONSTANTS)
              {
                vt->CmdSetBlendConstants(Unwrap(cmd), state.blendConst);
              }
              else if(d == VK_DYNAMIC_STATE_DEPTH_BOUNDS)
              {
                vt->CmdSetDepthBounds(Unwrap(cmd), state.mindepth, state.maxdepth);
              }
              else if(d == VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK)
              {
                vt->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT,
                                             state.back.compare);
                vt->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT,
                                             state.front.compare);
              }
              else if(d == VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)
              {
                vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.write);
                vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.write);
              }
              else if(d == VK_DYNAMIC_STATE_STENCIL_REFERENCE)
              {
                vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.ref);
                vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.ref);
              }
            }

            if(fmt.idxByteWidth)
            {
              VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
              if(fmt.idxByteWidth == 4)
                idxtype = VK_INDEX_TYPE_UINT32;

              if(fmt.idxbuf != ResourceId())
              {
                VkBuffer ib = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.idxbuf);

                vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), fmt.idxoffs, idxtype);
                vt->CmdDrawIndexed(Unwrap(cmd), fmt.numVerts, 1, 0, fmt.baseVertex, 0);
              }
            }
            else
            {
              vt->CmdDraw(Unwrap(cmd), fmt.numVerts, 1, 0, 0);
            }
          }
        }
      }

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      if(depthUsed)
      {
        m_pDriver->vkDestroyFramebuffer(m_Device, FB, NULL);
        m_pDriver->vkDestroyRenderPass(m_Device, RP, NULL);
      }

      for(auto it = pipes.begin(); it != pipes.end(); ++it)
        m_pDriver->vkDestroyPipeline(m_Device, it->second, NULL);
    }

    // restore back to normal
    m_pDriver->ReplayLog(0, eventID, eReplay_WithoutDraw);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  return GetResID(m_OverlayImage);
}

MeshDisplayPipelines VulkanDebugManager::CacheMeshDisplayPipelines(const MeshFormat &primary,
                                                                   const MeshFormat &secondary)
{
  // generate a key to look up the map
  uint64_t key = 0;

  uint64_t bit = 0;

  if(primary.idxByteWidth == 4)
    key |= 1ULL << bit;
  bit++;

  RDCASSERT((uint32_t)primary.topo < 64);
  key |= uint64_t((uint32_t)primary.topo & 0x3f) << bit;
  bit += 6;

  ResourceFormat fmt;
  fmt.special = primary.specialFormat != SpecialFormat::Unknown;
  fmt.specialFormat = primary.specialFormat;
  fmt.compByteWidth = primary.compByteWidth;
  fmt.compCount = primary.compCount;
  fmt.compType = primary.compType;

  VkFormat primaryFmt = MakeVkFormat(fmt);

  fmt.special = secondary.specialFormat != SpecialFormat::Unknown;
  fmt.specialFormat = secondary.specialFormat;
  fmt.compByteWidth = secondary.compByteWidth;
  fmt.compCount = secondary.compCount;
  fmt.compType = secondary.compType;

  VkFormat secondaryFmt = secondary.buf == ResourceId() ? VK_FORMAT_UNDEFINED : MakeVkFormat(fmt);

  RDCCOMPILE_ASSERT(VK_FORMAT_RANGE_SIZE <= 255,
                    "Mesh pipeline cache key needs an extra bit for format");

  key |= uint64_t((uint32_t)primaryFmt & 0xff) << bit;
  bit += 8;

  key |= uint64_t((uint32_t)secondaryFmt & 0xff) << bit;
  bit += 8;

  RDCASSERT(primary.stride <= 0xffff);
  key |= uint64_t((uint32_t)primary.stride & 0xffff) << bit;
  bit += 16;

  if(secondary.buf != ResourceId())
  {
    RDCASSERT(secondary.stride <= 0xffff);
    key |= uint64_t((uint32_t)secondary.stride & 0xffff) << bit;
  }
  bit += 16;

  MeshDisplayPipelines &cache = m_CachedMeshPipelines[key];

  if(cache.pipes[(uint32_t)SolidShade::NoSolid] != VK_NULL_HANDLE)
    return cache;

  const VkLayerDispatchTable *vt = ObjDisp(m_Device);
  VkResult vkr = VK_SUCCESS;

  // should we try and evict old pipelines from the cache here?
  // or just keep them forever

  VkVertexInputBindingDescription binds[] = {// primary
                                             {0, primary.stride, VK_VERTEX_INPUT_RATE_VERTEX},
                                             // secondary
                                             {1, secondary.stride, VK_VERTEX_INPUT_RATE_VERTEX}};

  RDCASSERT(primaryFmt != VK_FORMAT_UNDEFINED);

  VkVertexInputAttributeDescription vertAttrs[] = {
      // primary
      {
          0, 0, primaryFmt, 0,
      },
      // secondary
      {
          1, 0, primaryFmt, 0,
      },
  };

  VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0, 1, binds, 2, vertAttrs,
  };

  VkPipelineShaderStageCreateInfo stages[3] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_ALL_GRAPHICS,
       VK_NULL_HANDLE, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_ALL_GRAPHICS,
       VK_NULL_HANDLE, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_ALL_GRAPHICS,
       VK_NULL_HANDLE, "main", NULL},
  };

  VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
      primary.topo >= Topology::PatchList ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                                          : MakeVkPrimitiveTopology(primary.topo),
      false,
  };

  VkRect2D scissor = {{0, 0}, {16384, 16384}};

  VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, &scissor};

  VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      false,
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_NONE,
      VK_FRONT_FACE_CLOCKWISE,
      false,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
  };

  VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      NULL,
      0,
      VULKAN_MESH_VIEW_SAMPLES,
      false,
      0.0f,
      NULL,
      false,
      false};

  VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      NULL,
      0,
      true,
      true,
      VK_COMPARE_OP_LESS_OR_EQUAL,
      false,
      false,
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      0.0f,
      1.0f,
  };

  VkPipelineColorBlendAttachmentState attState = {
      false,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      0xf,
  };

  VkPipelineColorBlendStateCreateInfo cb = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      VK_LOGIC_OP_NO_OP,
      1,
      &attState,
      {1.0f, 1.0f, 1.0f, 1.0f}};

  VkDynamicState dynstates[] = {VK_DYNAMIC_STATE_VIEWPORT};

  VkPipelineDynamicStateCreateInfo dyn = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      NULL,
      0,
      ARRAY_COUNT(dynstates),
      dynstates,
  };

  VkRenderPass rp;    // compatible render pass

  {
    VkAttachmentDescription attDesc[] = {
        {0, VK_FORMAT_R8G8B8A8_SRGB, VULKAN_MESH_VIEW_SAMPLES, VK_ATTACHMENT_LOAD_OP_LOAD,
         VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
         VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {0, VK_FORMAT_D32_SFLOAT, VULKAN_MESH_VIEW_SAMPLES, VK_ATTACHMENT_LOAD_OP_LOAD,
         VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
         VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };

    VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dsRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub = {
        0,      VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,      NULL,       // inputs
        1,      &attRef,    // color
        NULL,               // resolve
        &dsRef,             // depth-stencil
        0,      NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        2,
        attDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    vt->CreateRenderPass(Unwrap(m_Device), &rpinfo, NULL, &rp);
  }

  VkGraphicsPipelineCreateInfo pipeInfo = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      NULL,
      0,
      2,
      stages,
      &vi,
      &ia,
      NULL,    // tess
      &vp,
      &rs,
      &msaa,
      &ds,
      &cb,
      &dyn,
      Unwrap(m_MeshPipeLayout),
      rp,
      0,                 // sub pass
      VK_NULL_HANDLE,    // base pipeline handle
      0,                 // base pipeline index
  };

  // wireframe pipeline
  stages[0].module = Unwrap(m_MeshModules[0]);
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[1].module = Unwrap(m_MeshModules[2]);
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  rs.polygonMode = VK_POLYGON_MODE_LINE;
  rs.lineWidth = 1.0f;
  ds.depthTestEnable = false;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_Wire]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ds.depthTestEnable = true;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // solid shading pipeline
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  ds.depthTestEnable = false;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_Solid]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ds.depthTestEnable = true;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(secondary.buf != ResourceId())
  {
    // pull secondary information from second vertex buffer
    vertAttrs[1].binding = 1;
    vertAttrs[1].format = secondaryFmt;
    RDCASSERT(secondaryFmt != VK_FORMAT_UNDEFINED);

    vi.vertexBindingDescriptionCount = 2;

    vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                      &cache.pipes[MeshDisplayPipelines::ePipe_Secondary]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  vertAttrs[1].binding = 0;
  vi.vertexBindingDescriptionCount = 1;

  // flat lit pipeline, needs geometry shader to calculate face normals
  stages[0].module = Unwrap(m_MeshModules[0]);
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[1].module = Unwrap(m_MeshModules[1]);
  stages[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
  stages[2].module = Unwrap(m_MeshModules[2]);
  stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  pipeInfo.stageCount = 3;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_Lit]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t i = 0; i < MeshDisplayPipelines::ePipe_Count; i++)
    if(cache.pipes[i] != VK_NULL_HANDLE)
      GetResourceManager()->WrapResource(Unwrap(m_Device), cache.pipes[i]);

  vt->DestroyRenderPass(Unwrap(m_Device), rp, NULL);

  return cache;
}

inline uint32_t MakeSPIRVOp(spv::Op op, uint32_t WordCount)
{
  return (uint32_t(op) & spv::OpCodeMask) | (WordCount << spv::WordCountShift);
}

static void AddOutputDumping(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                             const char *entryName, uint32_t &descSet, uint32_t vertexIndexOffset,
                             uint32_t instanceIndexOffset, uint32_t numVerts,
                             vector<uint32_t> &modSpirv, uint32_t &bufStride)
{
  uint32_t *spirv = &modSpirv[0];
  size_t spirvLength = modSpirv.size();

  int numOutputs = refl.OutputSig.count;

  RDCASSERT(numOutputs > 0);

  // save the id bound. We use this whenever we need to allocate ourselves
  // a new ID
  uint32_t idBound = spirv[3];

  // we do multiple passes through the SPIR-V to simplify logic, rather than
  // trying to do as few passes as possible.

  // first try to find a few IDs of things we know we'll probably need:
  // * gl_VertexID, gl_InstanceID (identified by a DecorationBuiltIn)
  // * Int32 type, signed and unsigned
  // * Float types, half, float and double
  // * Input Pointer to Int32 (for declaring gl_VertexID)
  // * UInt32 constants from 0 up to however many outputs we have
  // * The entry point we're after
  //
  // At the same time we find the highest descriptor set used and add a
  // new descriptor set binding on the end for our output buffer. This is
  // much easier than trying to add a new bind to an existing descriptor
  // set (which would cascade into a new descriptor set layout, new pipeline
  // layout, etc etc!). However, this might push us over the limit on number
  // of descriptor sets.
  //
  // we also note the index where decorations end, and the index where
  // functions start, for if we need to add new decorations or new
  // types/constants/global variables
  uint32_t vertidxID = 0;
  uint32_t instidxID = 0;
  uint32_t sint32ID = 0;
  uint32_t sint32PtrInID = 0;
  uint32_t uint32ID = 0;
  uint32_t halfID = 0;
  uint32_t floatID = 0;
  uint32_t doubleID = 0;
  uint32_t entryID = 0;

  struct outputIDs
  {
    uint32_t constID;         // constant ID for the index of this output
    uint32_t basetypeID;      // the type ID for this output. Must be present already by definition!
    uint32_t uniformPtrID;    // Uniform Pointer ID for this output. Used to write the output data
    uint32_t outputPtrID;     // Output Pointer ID for this output. Used to read the output data
  };
  outputIDs outs[100] = {};

  RDCASSERT(numOutputs < 100);

  size_t entryInterfaceOffset = 0;
  size_t entryWordCountOffset = 0;
  uint16_t entryWordCount = 0;
  size_t decorateOffset = 0;
  size_t typeVarOffset = 0;

  descSet = 0;

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = spirv[it] >> spv::WordCountShift;
    spv::Op opcode = spv::Op(spirv[it] & spv::OpCodeMask);

    // we will use the descriptor set immediately after the last set statically used by the shader.
    // This means we don't have to worry about if the descriptor set layout declares more sets which
    // might be invalid and un-bindable, we just trample over the next set that's unused
    if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationDescriptorSet)
      descSet = RDCMAX(descSet, spirv[it + 3] + 1);

    if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationBuiltIn &&
       spirv[it + 3] == spv::BuiltInVertexIndex)
      vertidxID = spirv[it + 1];

    if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationBuiltIn &&
       spirv[it + 3] == spv::BuiltInInstanceIndex)
      instidxID = spirv[it + 1];

    if(opcode == spv::OpTypeInt && spirv[it + 2] == 32 && spirv[it + 3] == 1)
      sint32ID = spirv[it + 1];

    if(opcode == spv::OpTypeInt && spirv[it + 2] == 32 && spirv[it + 3] == 0)
      uint32ID = spirv[it + 1];

    if(opcode == spv::OpTypeFloat && spirv[it + 2] == 16)
      halfID = spirv[it + 1];

    if(opcode == spv::OpTypeFloat && spirv[it + 2] == 32)
      floatID = spirv[it + 1];

    if(opcode == spv::OpTypeFloat && spirv[it + 2] == 64)
      doubleID = spirv[it + 1];

    if(opcode == spv::OpTypePointer && spirv[it + 2] == spv::StorageClassInput &&
       spirv[it + 3] == sint32ID)
      sint32PtrInID = spirv[it + 1];

    for(int i = 0; i < numOutputs; i++)
    {
      if(opcode == spv::OpConstant && spirv[it + 1] == uint32ID && spirv[it + 3] == (uint32_t)i)
      {
        if(outs[i].constID != 0)
          RDCWARN("identical constant declared with two different IDs %u %u!", spirv[it + 2],
                  outs[i].constID);    // not sure if this is valid or not
        outs[i].constID = spirv[it + 2];
      }

      if(outs[i].basetypeID == 0)
      {
        if(refl.OutputSig[i].compCount > 1 && opcode == spv::OpTypeVector)
        {
          uint32_t baseID = 0;

          if(refl.OutputSig[i].compType == CompType::UInt)
            baseID = uint32ID;
          else if(refl.OutputSig[i].compType == CompType::SInt)
            baseID = sint32ID;
          else if(refl.OutputSig[i].compType == CompType::Float)
            baseID = floatID;
          else if(refl.OutputSig[i].compType == CompType::Double)
            baseID = doubleID;
          else
            RDCERR("Unexpected component type for output signature element");

          // if we have the base type, see if this is the right sized vector of that type
          if(baseID != 0 && spirv[it + 2] == baseID && spirv[it + 3] == refl.OutputSig[i].compCount)
            outs[i].basetypeID = spirv[it + 1];
        }

        // handle non-vectors
        if(refl.OutputSig[i].compCount == 1)
        {
          if(refl.OutputSig[i].compType == CompType::UInt)
            outs[i].basetypeID = uint32ID;
          else if(refl.OutputSig[i].compType == CompType::SInt)
            outs[i].basetypeID = sint32ID;
          else if(refl.OutputSig[i].compType == CompType::Float)
            outs[i].basetypeID = floatID;
          else if(refl.OutputSig[i].compType == CompType::Double)
            outs[i].basetypeID = doubleID;
        }
      }

      // if we've found the base type, try and identify pointers to that type
      if(outs[i].basetypeID != 0 && opcode == spv::OpTypePointer &&
         spirv[it + 2] == spv::StorageClassUniform && spirv[it + 3] == outs[i].basetypeID)
      {
        outs[i].uniformPtrID = spirv[it + 1];
      }

      if(outs[i].basetypeID != 0 && opcode == spv::OpTypePointer &&
         spirv[it + 2] == spv::StorageClassOutput && spirv[it + 3] == outs[i].basetypeID)
      {
        outs[i].outputPtrID = spirv[it + 1];
      }
    }

    if(opcode == spv::OpEntryPoint)
    {
      const char *name = (const char *)&spirv[it + 3];

      if(!strcmp(name, entryName))
      {
        if(entryID != 0)
          RDCERR("Same entry point declared twice! %s", entryName);
        entryID = spirv[it + 2];
      }

      // need to update the WordCount when we add IDs, so store this
      entryWordCountOffset = it;
      entryWordCount = WordCount;

      // where to insert new interface IDs if we add them
      entryInterfaceOffset = it + WordCount;
    }

    // when we reach the types, decorations are over
    if(decorateOffset == 0 && opcode >= spv::OpTypeVoid && opcode <= spv::OpTypeForwardPointer)
      decorateOffset = it;

    // stop when we reach the functions, types are over
    if(opcode == spv::OpFunction)
    {
      typeVarOffset = it;
      break;
    }

    it += WordCount;
  }

  RDCASSERT(entryID != 0);

  for(int i = 0; i < numOutputs; i++)
  {
    // must have at least found the base type, or something has gone seriously wrong
    RDCASSERT(outs[i].basetypeID != 0);
  }

  // if needed add new ID for sint32 type
  if(sint32ID == 0)
  {
    sint32ID = idBound++;

    uint32_t typeOp[] = {
        MakeSPIRVOp(spv::OpTypeInt, 4), sint32ID,
        32U,    // 32-bit
        1U,     // signed
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(typeOp);
  }

  // if needed, new ID for input ptr type
  if(sint32PtrInID == 0 && (vertidxID == 0 || instidxID == 0))
  {
    sint32PtrInID = idBound;
    idBound++;

    uint32_t typeOp[] = {
        MakeSPIRVOp(spv::OpTypePointer, 4), sint32PtrInID, spv::StorageClassInput, sint32ID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(typeOp);
  }

  if(vertidxID == 0)
  {
    // need to declare our own "in int gl_VertexID;"

    // new ID for vertex index
    vertidxID = idBound;
    idBound++;

    uint32_t varOp[] = {
        MakeSPIRVOp(spv::OpVariable, 4),
        sint32PtrInID,    // type
        vertidxID,        // variable id
        spv::StorageClassInput,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, varOp, varOp + ARRAY_COUNT(varOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(varOp);

    uint32_t decorateOp[] = {
        MakeSPIRVOp(spv::OpDecorate, 4), vertidxID, spv::DecorationBuiltIn, spv::BuiltInVertexIndex,
    };

    // insert at the end of the decorations before the types
    modSpirv.insert(modSpirv.begin() + decorateOffset, decorateOp,
                    decorateOp + ARRAY_COUNT(decorateOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(decorateOp);
    decorateOffset += ARRAY_COUNT(decorateOp);

    modSpirv[entryWordCountOffset] = MakeSPIRVOp(spv::OpEntryPoint, ++entryWordCount);

    // need to add this input to the declared interface on OpEntryPoint
    modSpirv.insert(modSpirv.begin() + entryInterfaceOffset, vertidxID);

    // update offsets to account for inserted ID
    entryInterfaceOffset++;
    typeVarOffset++;
    decorateOffset++;
  }

  if(instidxID == 0)
  {
    // need to declare our own "in int gl_InstanceID;"

    // new ID for vertex index
    instidxID = idBound;
    idBound++;

    uint32_t varOp[] = {
        MakeSPIRVOp(spv::OpVariable, 4),
        sint32PtrInID,    // type
        instidxID,        // variable id
        spv::StorageClassInput,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, varOp, varOp + ARRAY_COUNT(varOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(varOp);

    uint32_t decorateOp[] = {
        MakeSPIRVOp(spv::OpDecorate, 4), instidxID, spv::DecorationBuiltIn, spv::BuiltInInstanceIndex,
    };

    // insert at the end of the decorations before the types
    modSpirv.insert(modSpirv.begin() + decorateOffset, decorateOp,
                    decorateOp + ARRAY_COUNT(decorateOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(decorateOp);
    decorateOffset += ARRAY_COUNT(decorateOp);

    modSpirv[entryWordCountOffset] = MakeSPIRVOp(spv::OpEntryPoint, ++entryWordCount);

    // need to add this input to the declared interface on OpEntryPoint
    modSpirv.insert(modSpirv.begin() + entryInterfaceOffset, instidxID);

    // update offsets to account for inserted ID
    entryInterfaceOffset++;
    typeVarOffset++;
    decorateOffset++;
  }

  // if needed add new ID for uint32 type
  if(uint32ID == 0)
  {
    uint32ID = idBound++;

    uint32_t typeOp[] = {
        MakeSPIRVOp(spv::OpTypeInt, 4), uint32ID,
        32U,    // 32-bit
        0U,     // unsigned
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(typeOp);
  }

  // add any constants we're missing
  for(int i = 0; i < numOutputs; i++)
  {
    if(outs[i].constID == 0)
    {
      outs[i].constID = idBound++;

      uint32_t constantOp[] = {
          MakeSPIRVOp(spv::OpConstant, 4), uint32ID, outs[i].constID, (uint32_t)i,
      };

      // insert at the end of the types/variables/constants section
      modSpirv.insert(modSpirv.begin() + typeVarOffset, constantOp,
                      constantOp + ARRAY_COUNT(constantOp));

      // update offsets to account for inserted op
      typeVarOffset += ARRAY_COUNT(constantOp);
    }
  }

  // add any uniform pointer types we're missing. Note that it's quite likely
  // output types will overlap (think - 5 outputs, 3 of which are float4/vec4)
  // so any time we create a new uniform pointer type, we update all subsequent
  // outputs to refer to it.
  for(int i = 0; i < numOutputs; i++)
  {
    if(outs[i].uniformPtrID == 0)
    {
      outs[i].uniformPtrID = idBound++;

      uint32_t typeOp[] = {
          MakeSPIRVOp(spv::OpTypePointer, 4), outs[i].uniformPtrID, spv::StorageClassUniform,
          outs[i].basetypeID,
      };

      // insert at the end of the types/variables/constants section
      modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

      // update offsets to account for inserted op
      typeVarOffset += ARRAY_COUNT(typeOp);

      // update subsequent outputs of identical type
      for(int j = i + 1; j < numOutputs; j++)
      {
        if(outs[i].basetypeID == outs[j].basetypeID)
        {
          RDCASSERT(outs[j].uniformPtrID == 0);
          outs[j].uniformPtrID = outs[i].uniformPtrID;
        }
      }
    }

    // it would be very strange to have no output pointer ID, since the original SPIR-V would have
    // had to use some other mechanism to write to the output variable. But just to be safe we
    // ensure that we have it here too.
    if(outs[i].outputPtrID == 0)
    {
      RDCERR("No output pointer ID found for output %d: %s (%u %u)", i,
             refl.OutputSig[i].varName.c_str(), refl.OutputSig[i].compType,
             refl.OutputSig[i].compCount);

      outs[i].outputPtrID = idBound++;

      uint32_t typeOp[] = {
          MakeSPIRVOp(spv::OpTypePointer, 4), outs[i].outputPtrID, spv::StorageClassOutput,
          outs[i].basetypeID,
      };

      // insert at the end of the types/variables/constants section
      modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

      // update offsets to account for inserted op
      typeVarOffset += ARRAY_COUNT(typeOp);

      // update subsequent outputs of identical type
      for(int j = i + 1; j < numOutputs; j++)
      {
        if(outs[i].basetypeID == outs[j].basetypeID)
        {
          RDCASSERT(outs[j].outputPtrID == 0);
          outs[j].outputPtrID = outs[i].outputPtrID;
        }
      }
    }
  }

  uint32_t outBufferVarID = 0;
  uint32_t numVertsConstID = 0;
  uint32_t vertexIndexOffsetConstID = 0;
  uint32_t instanceIndexOffsetConstID = 0;

  // now add the structure type etc for our output buffer
  {
    uint32_t vertStructID = idBound++;

    uint32_t vertStructOp[2 + 100] = {
        MakeSPIRVOp(spv::OpTypeStruct, 2 + numOutputs), vertStructID,
    };

    for(int o = 0; o < numOutputs; o++)
      vertStructOp[2 + o] = outs[o].basetypeID;

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, vertStructOp, vertStructOp + 2 + numOutputs);

    // update offsets to account for inserted op
    typeVarOffset += 2 + numOutputs;

    uint32_t runtimeArrayID = idBound++;

    uint32_t runtimeArrayOp[] = {
        MakeSPIRVOp(spv::OpTypeRuntimeArray, 3), runtimeArrayID, vertStructID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, runtimeArrayOp,
                    runtimeArrayOp + ARRAY_COUNT(runtimeArrayOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(runtimeArrayOp);

    // add a constant for the number of verts, the 'instance stride' of the array
    numVertsConstID = idBound++;

    uint32_t instanceStrideConstOp[] = {
        MakeSPIRVOp(spv::OpConstant, 4), sint32ID, numVertsConstID, numVerts,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, instanceStrideConstOp,
                    instanceStrideConstOp + ARRAY_COUNT(instanceStrideConstOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(instanceStrideConstOp);

    // add a constant for the value that VertexIndex starts at, so we can get a 0-based vertex index
    vertexIndexOffsetConstID = idBound++;

    uint32_t vertexIndexOffsetConstOp[] = {
        MakeSPIRVOp(spv::OpConstant, 4), sint32ID, vertexIndexOffsetConstID, vertexIndexOffset,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, vertexIndexOffsetConstOp,
                    vertexIndexOffsetConstOp + ARRAY_COUNT(vertexIndexOffsetConstOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(vertexIndexOffsetConstOp);

    // add a constant for the value that InstanceIndex starts at, so we can get a 0-based instance
    // index
    instanceIndexOffsetConstID = idBound++;

    uint32_t instanceIndexOffsetConstOp[] = {
        MakeSPIRVOp(spv::OpConstant, 4), sint32ID, instanceIndexOffsetConstID, instanceIndexOffset,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, instanceIndexOffsetConstOp,
                    instanceIndexOffsetConstOp + ARRAY_COUNT(instanceIndexOffsetConstOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(instanceIndexOffsetConstOp);

    uint32_t outputStructID = idBound++;

    uint32_t outputStructOp[] = {
        MakeSPIRVOp(spv::OpTypeStruct, 3), outputStructID, runtimeArrayID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, outputStructOp,
                    outputStructOp + ARRAY_COUNT(outputStructOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(outputStructOp);

    uint32_t outputStructPtrID = idBound++;

    uint32_t outputStructPtrOp[] = {
        MakeSPIRVOp(spv::OpTypePointer, 4), outputStructPtrID, spv::StorageClassUniform,
        outputStructID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, outputStructPtrOp,
                    outputStructPtrOp + ARRAY_COUNT(outputStructPtrOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(outputStructPtrOp);

    outBufferVarID = idBound++;

    uint32_t outputVarOp[] = {
        MakeSPIRVOp(spv::OpVariable, 4), outputStructPtrID, outBufferVarID, spv::StorageClassUniform,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, outputVarOp,
                    outputVarOp + ARRAY_COUNT(outputVarOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(outputVarOp);

    // need to add decorations as appropriate
    vector<uint32_t> decorations;

    // reserve room for 1 member decorate per output, plus
    // other fixed decorations
    decorations.reserve(5 * numOutputs + 20);

    uint32_t memberOffset = 0;
    for(int o = 0; o < numOutputs; o++)
    {
      uint32_t elemSize = 0;
      if(refl.OutputSig[o].compType == CompType::Double)
        elemSize = 8;
      else if(refl.OutputSig[o].compType == CompType::SInt ||
              refl.OutputSig[o].compType == CompType::UInt ||
              refl.OutputSig[o].compType == CompType::Float)
        elemSize = 4;
      else
        RDCERR("Unexpected component type for output signature element");

      uint32_t numComps = refl.OutputSig[o].compCount;

      // ensure member is std430 packed (vec4 alignment for vec3/vec4)
      if(numComps == 2)
        memberOffset = AlignUp(memberOffset, 2U * elemSize);
      else if(numComps > 2)
        memberOffset = AlignUp(memberOffset, 4U * elemSize);

      decorations.push_back(MakeSPIRVOp(spv::OpMemberDecorate, 5));
      decorations.push_back(vertStructID);
      decorations.push_back((uint32_t)o);
      decorations.push_back(spv::DecorationOffset);
      decorations.push_back(memberOffset);

      memberOffset += elemSize * refl.OutputSig[o].compCount;
    }

    // align to 16 bytes (vec4) since we will almost certainly have
    // a vec4 in the struct somewhere, and even in std430 alignment,
    // the base struct alignment is still the largest base alignment
    // of any member
    memberOffset = AlignUp16(memberOffset);

    // the array is the only element in the output struct, so
    // it's at offset 0
    decorations.push_back(MakeSPIRVOp(spv::OpMemberDecorate, 5));
    decorations.push_back(outputStructID);
    decorations.push_back(0);
    decorations.push_back(spv::DecorationOffset);
    decorations.push_back(0);

    // set array stride
    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 4));
    decorations.push_back(runtimeArrayID);
    decorations.push_back(spv::DecorationArrayStride);
    decorations.push_back(memberOffset);

    bufStride = memberOffset;

    // set object type
    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 3));
    decorations.push_back(outputStructID);
    decorations.push_back(spv::DecorationBufferBlock);

    // set binding
    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 4));
    decorations.push_back(outBufferVarID);
    decorations.push_back(spv::DecorationDescriptorSet);
    decorations.push_back(descSet);

    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 4));
    decorations.push_back(outBufferVarID);
    decorations.push_back(spv::DecorationBinding);
    decorations.push_back(0);

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + decorateOffset, decorations.begin(), decorations.end());

    // update offsets to account for inserted op
    typeVarOffset += decorations.size();
    decorateOffset += decorations.size();
  }

  vector<uint32_t> dumpCode;

  {
    // bit of a conservative resize. Each output if in a struct could have
    // AccessChain on source = 4 uint32s
    // Load source           = 4 uint32s
    // AccessChain on dest   = 7 uint32s
    // Store dest            = 3 uint32s
    //
    // loading the indices, and multiplying to get the destination array
    // slot is constant on top of that
    dumpCode.reserve(numOutputs * (4 + 4 + 7 + 3) + 4 + 4 + 5 + 5);

    uint32_t loadedVtxID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(loadedVtxID);
    dumpCode.push_back(vertidxID);

    uint32_t loadedInstID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(loadedInstID);
    dumpCode.push_back(instidxID);

    uint32_t rebasedInstID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpISub, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(rebasedInstID);                 // rebasedInst =
    dumpCode.push_back(loadedInstID);                  //    gl_InstanceIndex -
    dumpCode.push_back(instanceIndexOffsetConstID);    //    instanceIndexOffset

    uint32_t startVertID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpIMul, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(startVertID);        // startVert =
    dumpCode.push_back(rebasedInstID);      //    rebasedInst *
    dumpCode.push_back(numVertsConstID);    //    numVerts

    uint32_t rebasedVertID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpISub, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(rebasedVertID);               // rebasedVert =
    dumpCode.push_back(loadedVtxID);                 //    gl_VertexIndex -
    dumpCode.push_back(vertexIndexOffsetConstID);    //    vertexIndexOffset

    uint32_t arraySlotID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpIAdd, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(arraySlotID);      // arraySlot =
    dumpCode.push_back(startVertID);      //    startVert +
    dumpCode.push_back(rebasedVertID);    //    rebasedVert

    for(int o = 0; o < numOutputs; o++)
    {
      uint32_t loaded = 0;

      // not a structure member or array child, can load directly
      if(patchData.outputs[o].accessChain.empty())
      {
        loaded = idBound++;

        dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
        dumpCode.push_back(outs[o].basetypeID);
        dumpCode.push_back(loaded);
        dumpCode.push_back(patchData.outputs[o].ID);
      }
      else
      {
        uint32_t readPtr = idBound++;
        loaded = idBound++;

        // structure member, need to access chain first
        dumpCode.push_back(
            MakeSPIRVOp(spv::OpAccessChain, 4 + (uint32_t)patchData.outputs[o].accessChain.size()));
        dumpCode.push_back(outs[o].outputPtrID);
        dumpCode.push_back(readPtr);                    // readPtr =
        dumpCode.push_back(patchData.outputs[o].ID);    // outStructWhatever

        for(uint32_t idx : patchData.outputs[o].accessChain)
          dumpCode.push_back(outs[idx].constID);

        dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
        dumpCode.push_back(outs[o].basetypeID);
        dumpCode.push_back(loaded);
        dumpCode.push_back(readPtr);
      }

      // access chain the destination
      uint32_t writePtr = idBound++;
      dumpCode.push_back(MakeSPIRVOp(spv::OpAccessChain, 7));
      dumpCode.push_back(outs[o].uniformPtrID);
      dumpCode.push_back(writePtr);
      dumpCode.push_back(outBufferVarID);     // outBuffer
      dumpCode.push_back(outs[0].constID);    // .verts
      dumpCode.push_back(arraySlotID);        // [arraySlot]
      dumpCode.push_back(outs[o].constID);    // .out_...

      dumpCode.push_back(MakeSPIRVOp(spv::OpStore, 3));
      dumpCode.push_back(writePtr);
      dumpCode.push_back(loaded);
    }
  }

  // update these values, since vector will have resized and/or reallocated above
  spirv = &modSpirv[0];
  spirvLength = modSpirv.size();

  bool infunc = false;

  it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = spirv[it] >> spv::WordCountShift;
    spv::Op opcode = spv::Op(spirv[it] & spv::OpCodeMask);

    // find the start of the entry point
    if(opcode == spv::OpFunction && spirv[it + 2] == entryID)
      infunc = true;

    // insert the dumpCode before any spv::OpReturn.
    // we should not have any spv::OpReturnValue since this is
    // the entry point. Neither should we have OpKill etc.
    if(infunc && opcode == spv::OpReturn)
    {
      modSpirv.insert(modSpirv.begin() + it, dumpCode.begin(), dumpCode.end());

      it += dumpCode.size();

      // update these values, since vector will have resized and/or reallocated above
      spirv = &modSpirv[0];
      spirvLength = modSpirv.size();
    }

    // done patching entry point
    if(opcode == spv::OpFunctionEnd && infunc)
      break;

    it += WordCount;
  }

  // patch up the new id bound
  spirv[3] = idBound;
}

void VulkanDebugManager::InitPostVSBuffers(uint32_t eventID)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventID) != m_PostVSAlias.end())
    eventID = m_PostVSAlias[eventID];

  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    return;

  if(!m_pDriver->GetDeviceFeatures().vertexPipelineStoresAndAtomics)
    return;

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  if(state.graphics.pipeline == ResourceId() || state.renderPass == ResourceId())
    return;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  if(pipeInfo.shaders[0].module == ResourceId())
    return;

  const VulkanCreationInfo::ShaderModule &moduleInfo =
      creationInfo.m_ShaderModule[pipeInfo.shaders[0].module];

  ShaderReflection *refl = pipeInfo.shaders[0].refl;

  // no outputs from this shader? unexpected but theoretically possible (dummy VS before
  // tessellation maybe). Just fill out an empty data set
  if(refl->OutputSig.count == 0)
  {
    // empty vertex output signature
    m_PostVSData[eventID].vsin.topo = pipeInfo.topology;
    m_PostVSData[eventID].vsout.buf = VK_NULL_HANDLE;
    m_PostVSData[eventID].vsout.instStride = 0;
    m_PostVSData[eventID].vsout.vertStride = 0;
    m_PostVSData[eventID].vsout.nearPlane = 0.0f;
    m_PostVSData[eventID].vsout.farPlane = 0.0f;
    m_PostVSData[eventID].vsout.useIndices = false;
    m_PostVSData[eventID].vsout.hasPosOut = false;
    m_PostVSData[eventID].vsout.idxBuf = VK_NULL_HANDLE;

    m_PostVSData[eventID].vsout.topo = pipeInfo.topology;

    return;
  }

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventID);

  if(drawcall == NULL || drawcall->numIndices == 0 || drawcall->numInstances == 0)
    return;

  // the SPIR-V patching will determine the next descriptor set to use, after all sets statically
  // used by the shader. This gets around the problem where the shader only uses 0 and 1, but the
  // layout declares 0-4, and 2,3,4 are invalid at bind time and we are unable to bind our new set
  // 5. Instead we'll notice that only 0 and 1 are used and just use 2 ourselves (although it was in
  // the original set layout, we know it's statically unused by the shader so we can safely steal
  // it).
  uint32_t descSet = 0;

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkPipelineLayout pipeLayout;

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  // set primitive topology to point list
  VkPipelineInputAssemblyStateCreateInfo *ia =
      (VkPipelineInputAssemblyStateCreateInfo *)pipeCreateInfo.pInputAssemblyState;

  VkPrimitiveTopology topo = ia->topology;

  ia->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

  // remove all stages but the vertex shader, we just want to run it and write the data,
  // we don't want to tessellate/geometry shade, nor rasterize (which we disable below)
  uint32_t vertIdx = pipeCreateInfo.stageCount;

  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    if(pipeCreateInfo.pStages[i].stage & VK_SHADER_STAGE_VERTEX_BIT)
    {
      vertIdx = i;
      break;
    }
  }

  RDCASSERT(vertIdx < pipeCreateInfo.stageCount);

  if(vertIdx != 0)
    (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[0] = pipeCreateInfo.pStages[vertIdx];

  pipeCreateInfo.stageCount = 1;

  // enable rasterizer discard
  VkPipelineRasterizationStateCreateInfo *rs =
      (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
  rs->rasterizerDiscardEnable = true;

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkBuffer idxBuf = VK_NULL_HANDLE, uniqIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory idxBufMem = VK_NULL_HANDLE, uniqIdxBufMem = VK_NULL_HANDLE;

  uint32_t numVerts = drawcall->numIndices;
  VkDeviceSize bufSize = 0;

  vector<uint32_t> indices;
  uint32_t idxsize = state.ibuffer.bytewidth;
  bool index16 = (idxsize == 2);
  uint32_t numIndices = numVerts;
  vector<byte> idxdata;
  uint16_t *idx16 = NULL;
  uint32_t *idx32 = NULL;

  uint32_t minIndex = 0, maxIndex = 0;

  uint32_t vertexIndexOffset = 0;

  if(drawcall->flags & DrawFlags::UseIBuffer)
  {
    // fetch ibuffer
    GetBufferData(state.ibuffer.buf, state.ibuffer.offs + drawcall->indexOffset * idxsize,
                  drawcall->numIndices * idxsize, idxdata);

    // figure out what the maximum index could be, so we can clamp our index buffer to something
    // sane
    uint32_t maxIdx = 0;

    // if there are no active bindings assume the vertex shader is generating its own data
    // and don't clamp the indices
    if(pipeCreateInfo.pVertexInputState->vertexBindingDescriptionCount == 0)
      maxIdx = ~0U;

    for(uint32_t b = 0; b < pipeCreateInfo.pVertexInputState->vertexBindingDescriptionCount; b++)
    {
      const VkVertexInputBindingDescription &input =
          pipeCreateInfo.pVertexInputState->pVertexBindingDescriptions[b];
      // only vertex inputs (not instance inputs) count
      if(input.inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
      {
        if(b >= state.vbuffers.size())
          continue;

        ResourceId buf = state.vbuffers[b].buf;
        VkDeviceSize offs = state.vbuffers[b].offs;

        VkDeviceSize bufsize = creationInfo.m_Buffer[buf].size;

        // the maximum valid index on this particular input is the one that reaches
        // the end of the buffer. The maximum valid index at all is the one that reads
        // off the end of ALL buffers (so we max it with any other maxindex value
        // calculated).
        if(input.stride > 0)
          maxIdx = RDCMAX(maxIdx, uint32_t((bufsize - offs) / input.stride));
      }
    }

    // in case the vertex buffers were set but had invalid stride (0), max with the number
    // of vertices too. This is fine since the max here is just a conservative limit
    maxIdx = RDCMAX(maxIdx, drawcall->numIndices);

    // do ibuffer rebasing/remapping

    idx16 = (uint16_t *)&idxdata[0];
    idx32 = (uint32_t *)&idxdata[0];

    // only read as many indices as were available in the buffer
    numIndices =
        RDCMIN(uint32_t(index16 ? idxdata.size() / 2 : idxdata.size() / 4), drawcall->numIndices);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

      // we clamp to maxIdx here, to avoid any invalid indices like 0xffffffff
      // from filtering through. Worst case we index to the end of the vertex
      // buffers which is generally much more reasonable
      i32 = RDCMIN(maxIdx, i32);

      auto it = std::lower_bound(indices.begin(), indices.end(), i32);

      if(it != indices.end() && *it == i32)
        continue;

      indices.insert(it, i32);
    }

    // if we read out of bounds, we'll also have a 0 index being referenced
    // (as 0 is read). Don't insert 0 if we already have 0 though
    if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
      indices.insert(indices.begin(), 0);

    minIndex = indices[0];
    maxIndex = indices[indices.size() - 1];

    vertexIndexOffset = minIndex + drawcall->baseVertex;

    // set numVerts
    numVerts = maxIndex - minIndex + 1;

    // create buffer with unique 0-based indices
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &uniqIdxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, uniqIdxBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &uniqIdxBufMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, uniqIdxBuf, uniqIdxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, uniqIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, &indices[0], indices.size() * sizeof(uint32_t));

    m_pDriver->vkUnmapMemory(m_Device, uniqIdxBufMem);

    bufInfo.size = numIndices * idxsize;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &idxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, idxBuf, &mrq);

    allocInfo.allocationSize = mrq.size;
    allocInfo.memoryTypeIndex = m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &idxBufMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, idxBuf, idxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else
  {
    // firstVertex
    vertexIndexOffset = drawcall->vertexOffset;
  }

  uint32_t bufStride = 0;
  vector<uint32_t> modSpirv = moduleInfo.spirv.spirv;

  AddOutputDumping(*refl, *pipeInfo.shaders[0].patchData, pipeInfo.shaders[0].entryPoint.c_str(),
                   descSet, vertexIndexOffset, drawcall->instanceOffset, numVerts, modSpirv,
                   bufStride);

  {
    VkDescriptorSetLayout *descSetLayouts;

    // descSet will be the index of our new descriptor set
    descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

    for(uint32_t i = 0; i < descSet; i++)
      descSetLayouts[i] = GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
          creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts[i]);

    // this layout just says it has one storage buffer
    descSetLayouts[descSet] = m_MeshFetchDescSetLayout;

    const vector<VkPushConstantRange> &push =
        creationInfo.m_PipelineLayout[pipeInfo.layout].pushRanges;

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        descSet + 1,
        descSetLayouts,
        (uint32_t)push.size(),
        push.empty() ? NULL : &push[0],
    };

    // create pipeline layout with same descriptor set layouts, plus our mesh output set
    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    SAFE_DELETE_ARRAY(descSetLayouts);

    // repoint pipeline layout
    pipeCreateInfo.layout = pipeLayout;
  }

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkShaderModule module;
  vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // change vertex shader to use our modified code
  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &sh =
        (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
    if(sh.stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
      sh.module = module;
      // entry point name remains the same
      break;
    }
  }

  // create new pipeline
  VkPipeline pipe;
  vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                             &pipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.graphics.pipeline = GetResID(pipe);

  // push back extra descriptor set to partial replay state
  // note that we examined the used pipeline layout above and inserted our descriptor set
  // after any the application used. So there might be more bound, but we want to ensure to
  // bind to the slot we're using
  modifiedstate.graphics.descSets.resize(descSet + 1);
  modifiedstate.graphics.descSets[descSet].descSet = GetResID(m_MeshFetchDescSet);

  if(!(drawcall->flags & DrawFlags::UseIBuffer))
  {
    // create buffer of sufficient size (num indices * bufStride)
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        drawcall->numIndices * drawcall->numInstances * bufStride,
        0,
    };

    bufSize = bufInfo.size;

    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, m_MeshFetchDescSet, 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,         NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
    ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                          drawcall->vertexOffset, drawcall->instanceOffset);
    modifiedstate.EndRenderPass(cmd);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(meshBuffer),
        0,
        bufInfo.size,
    };

    // wait for writing to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0, 0, bufInfo.size,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }
  else
  {
    // create buffer of sufficient size
    // this can't just be bufStride * num unique indices per instance, as we don't
    // have a compact 0-based index to index into the buffer. We must use
    // index-minIndex which is 0-based but potentially sparse, so this buffer may
    // be more or less wasteful
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,          NULL, 0,
        numVerts * drawcall->numInstances * bufStride, 0,
    };

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_HOST_WRITE_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(uniqIdxBuf),
        0,
        indices.size() * sizeof(uint32_t),
    };

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // wait for upload to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufInfo.size, 0);

    // wait to finish
    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufInfo.size;
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // set bufSize
    bufSize = numVerts * drawcall->numInstances * bufStride;

    // bind unique'd ibuffer
    modifiedstate.ibuffer.bytewidth = 4;
    modifiedstate.ibuffer.offs = 0;
    modifiedstate.ibuffer.buf = GetResID(uniqIdxBuf);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, m_MeshFetchDescSet, 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,         NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
    ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), (uint32_t)indices.size(), drawcall->numInstances, 0,
                                 drawcall->baseVertex, drawcall->instanceOffset);
    modifiedstate.EndRenderPass(cmd);

    // rebase existing index buffer to point to the right elements in our stream-out'd
    // vertex buffer

    // An index buffer could be something like: 500, 520, 518, 553, 554, 556
    // in which case we can't use the existing index buffer without filling 499 slots of vertex
    // data with padding. Instead we rebase the indices based on the smallest index so it becomes
    // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
    //
    // Note that there could also be gaps in the indices as above which must remain as
    // we don't have a 0-based dense 'vertex id' to base our SSBO indexing off, only index value.

    bool stripRestart = pipeCreateInfo.pInputAssemblyState->primitiveRestartEnable == VK_TRUE &&
                        IsStrip(drawcall->topology);

    if(index16)
    {
      for(uint32_t i = 0; i < numIndices; i++)
      {
        if(stripRestart && idx16[i] == 0xffff)
          continue;

        idx16[i] = idx16[i] - uint16_t(minIndex);
      }
    }
    else
    {
      for(uint32_t i = 0; i < numIndices; i++)
      {
        if(stripRestart && idx32[i] == 0xffffffff)
          continue;

        idx32[i] -= minIndex;
      }
    }

    // upload rebased memory
    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, idxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, idx32, numIndices * idxsize);

    m_pDriver->vkUnmapMemory(m_Device, idxBufMem);

    meshbufbarrier.buffer = Unwrap(idxBuf);
    meshbufbarrier.size = numIndices * idxsize;

    // wait for upload to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // wait for mesh output writing to finish
    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufSize;
    meshbufbarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0, 0, bufInfo.size,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  // readback mesh data
  byte *byteData = NULL;
  vkr = m_pDriver->vkMapMemory(m_Device, readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&byteData);

  // do near/far calculations

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f *pos0 = (Vec4f *)byteData;

  bool found = false;

  // expect position at the start of the buffer, as system values are sorted first
  // and position is the first value

  for(uint32_t i = 1; refl->OutputSig[0].systemValue == ShaderBuiltin::Position && i < numVerts; i++)
  {
    //////////////////////////////////////////////////////////////////////////////////
    // derive near/far, assuming a standard perspective matrix
    //
    // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
    // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
    // and we know Wpost = Zpre from the perspective matrix.
    // we can then see from the perspective matrix that
    // m = F/(F-N)
    // c = -(F*N)/(F-N)
    //
    // with re-arranging and substitution, we then get:
    // N = -c/m
    // F = c/(1-m)
    //
    // so if we can derive m and c then we can determine N and F. We can do this with
    // two points, and we pick them reasonably distinct on z to reduce floating-point
    // error

    Vec4f *pos = (Vec4f *)(byteData + i * bufStride);

    // skip invalid vertices (w=0)
    if(pos->w != 0.0f && fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
    {
      Vec2f A(pos0->w, pos0->z);
      Vec2f B(pos->w, pos->z);

      float m = (B.y - A.y) / (B.x - A.x);
      float c = B.y - B.x * m;

      if(m == 1.0f)
        continue;

      if(-c / m <= 0.000001f)
        continue;

      nearp = -c / m;
      farp = c / (1 - m);

      found = true;

      break;
    }
  }

  // if we didn't find anything, all z's and w's were identical.
  // If the z is positive and w greater for the first element then
  // we detect this projection as reversed z with infinite far plane
  if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
  {
    nearp = pos0->z;
    farp = FLT_MAX;
  }

  m_pDriver->vkUnmapMemory(m_Device, readbackMem);

  // clean up temporary memories
  m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
  m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

  if(uniqIdxBuf != VK_NULL_HANDLE)
  {
    m_pDriver->vkDestroyBuffer(m_Device, uniqIdxBuf, NULL);
    m_pDriver->vkFreeMemory(m_Device, uniqIdxBufMem, NULL);
  }

  // fill out m_PostVSData
  m_PostVSData[eventID].vsin.topo = topo;
  m_PostVSData[eventID].vsout.topo = topo;
  m_PostVSData[eventID].vsout.buf = meshBuffer;
  m_PostVSData[eventID].vsout.bufmem = meshMem;

  m_PostVSData[eventID].vsout.vertStride = bufStride;
  m_PostVSData[eventID].vsout.nearPlane = nearp;
  m_PostVSData[eventID].vsout.farPlane = farp;

  m_PostVSData[eventID].vsout.useIndices = bool(drawcall->flags & DrawFlags::UseIBuffer);
  m_PostVSData[eventID].vsout.numVerts = drawcall->numIndices;

  m_PostVSData[eventID].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVSData[eventID].vsout.instStride = uint32_t(bufSize / drawcall->numInstances);

  m_PostVSData[eventID].vsout.idxBuf = VK_NULL_HANDLE;
  if(m_PostVSData[eventID].vsout.useIndices && idxBuf != VK_NULL_HANDLE)
  {
    m_PostVSData[eventID].vsout.idxBuf = idxBuf;
    m_PostVSData[eventID].vsout.idxBufMem = idxBufMem;
    m_PostVSData[eventID].vsout.idxFmt =
        state.ibuffer.bytewidth == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  }

  m_PostVSData[eventID].vsout.hasPosOut = refl->OutputSig[0].systemValue == ShaderBuiltin::Position;

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}

MeshFormat VulkanDebugManager::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventID) != m_PostVSAlias.end())
    eventID = m_PostVSAlias[eventID];

  VulkanPostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    postvs = m_PostVSData[eventID];

  VulkanPostVSData::StageData s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf != VK_NULL_HANDLE)
  {
    ret.idxbuf = GetResID(s.idxBuf);
    ret.idxByteWidth = s.idxFmt == VK_INDEX_TYPE_UINT16 ? 2 : 4;
  }
  else
  {
    ret.idxbuf = ResourceId();
    ret.idxByteWidth = 0;
  }
  ret.idxoffs = 0;
  ret.baseVertex = 0;

  if(s.buf != VK_NULL_HANDLE)
    ret.buf = GetResID(s.buf);
  else
    ret.buf = ResourceId();

  ret.offset = s.instStride * instID;
  ret.stride = s.vertStride;

  ret.compCount = 4;
  ret.compByteWidth = 4;
  ret.compType = CompType::Float;
  ret.specialFormat = SpecialFormat::Unknown;

  ret.showAlpha = false;
  ret.bgraOrder = false;

  ret.topo = MakePrimitiveTopology(s.topo, 1);
  ret.numVerts = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  return ret;
}
