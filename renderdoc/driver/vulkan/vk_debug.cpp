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

#include "vk_debug.h"
#include <float.h>
#include "core/settings.h"
#include "data/glsl_shaders.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/amd/official/GPUPerfAPI/Include/GPUPerfAPI-VK.h"
#include "driver/shaders/spirv/spirv_compile.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

RDOC_CONFIG(bool, Vulkan_HardwareCounters, true,
            "Enable support for IHV-specific hardware counters on Vulkan.");

const VkDeviceSize STAGE_BUFFER_BYTE_SIZE = 16 * 1024 * 1024ULL;

static void create(WrappedVulkan *driver, const char *objName, const int line, VkSampler *sampler,
                   VkFilter samplerFilter)
{
  VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampInfo.minFilter = sampInfo.magFilter = samplerFilter;
  sampInfo.mipmapMode = samplerFilter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                                           : VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.maxLod = 128.0f;

  VkResult vkr = driver->vkCreateSampler(driver->GetDev(), &sampInfo, NULL, sampler);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());
}

static void create(WrappedVulkan *driver, const char *objName, const int line,
                   VkDescriptorSetLayout *descLayout,
                   std::initializer_list<VkDescriptorSetLayoutBinding> bindings)
{
  VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      NULL,
      0,
      (uint32_t)bindings.size(),
      bindings.begin(),
  };

  VkResult vkr =
      driver->vkCreateDescriptorSetLayout(driver->GetDev(), &descsetLayoutInfo, NULL, descLayout);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());
}

static void create(WrappedVulkan *driver, const char *objName, const int line,
                   VkPipelineLayout *pipeLayout, VkDescriptorSetLayout setLayout, uint32_t pushBytes)
{
  VkPipelineLayoutCreateInfo pipeLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

  VkPushConstantRange push = {VK_SHADER_STAGE_ALL, 0, pushBytes};

  if(pushBytes > 0)
  {
    pipeLayoutInfo.pPushConstantRanges = &push;
    pipeLayoutInfo.pushConstantRangeCount = 1;
  }

  pipeLayoutInfo.pSetLayouts = &setLayout;
  pipeLayoutInfo.setLayoutCount = 1;

  VkResult vkr = driver->vkCreatePipelineLayout(driver->GetDev(), &pipeLayoutInfo, NULL, pipeLayout);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());
}

// create a single subpass renderpass with a single attachment
static void create(WrappedVulkan *driver, const char *objName, const int line,
                   VkRenderPass *renderPass, VkFormat attachFormat,
                   VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
                   VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
{
  VkAttachmentDescription attDesc = {0,
                                     attachFormat,
                                     sampleCount,
                                     VK_ATTACHMENT_LOAD_OP_LOAD,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                     VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                     layout,
                                     layout};

  VkAttachmentReference attRef = {0, layout};

  VkSubpassDescription sub = {
      0, VK_PIPELINE_BIND_POINT_GRAPHICS,
      0, NULL,       // inputs
      1, &attRef,    // color
  };

  if(IsDepthOrStencilFormat(attachFormat))
  {
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

    sub.colorAttachmentCount = 0;
    sub.pColorAttachments = NULL;
    sub.pDepthStencilAttachment = &attRef;
  }

  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &attDesc, 1, &sub,
  };

  VkResult vkr = driver->vkCreateRenderPass(driver->GetDev(), &rpinfo, NULL, renderPass);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());

  driver->GetResourceManager()->SetInternalResource(GetResID(*renderPass));
}

// Create a compute pipeline with a shader module
static void create(WrappedVulkan *driver, const char *objName, const int line, VkPipeline *pipe,
                   VkPipelineLayout pipeLayout, VkShaderModule computeModule)
{
  // if the module didn't compile, this pipeline is not be supported. Silently don't create it, code
  // later should handle the missing pipeline as indicating lack of support
  if(computeModule == VK_NULL_HANDLE)
  {
    *pipe = VK_NULL_HANDLE;
    return;
  }

  VkComputePipelineCreateInfo compPipeInfo = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      NULL,
      0,
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_COMPUTE_BIT,
       computeModule, "main", NULL},
      pipeLayout,
      VK_NULL_HANDLE,
      0,
  };

  VkResult vkr = driver->vkCreateComputePipelines(
      driver->GetDev(), driver->GetShaderCache()->GetPipeCache(), 1, &compPipeInfo, NULL, pipe);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());
}

static void create(WrappedVulkan *driver, const char *objName, const int line,
                   VkDescriptorSet *descSet, VkDescriptorPool pool, VkDescriptorSetLayout setLayout)
{
  VkDescriptorSetAllocateInfo descSetAllocInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL, pool, 1, &setLayout,
  };

  // don't expect this to fail (or if it does then it should be immediately obvious, not transient).
  VkResult vkr = driver->vkAllocateDescriptorSets(driver->GetDev(), &descSetAllocInfo, descSet);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());
}

// a simpler one-shot descriptor containing anything we might want to vary in a graphics pipeline
struct ConciseGraphicsPipeline
{
  // misc
  VkRenderPass renderPass;
  VkPipelineLayout pipeLayout;
  VkShaderModule vertex;
  VkShaderModule fragment;

  // dynamic state
  std::initializer_list<VkDynamicState> dynstates;

  // msaa
  VkSampleCountFlagBits sampleCount;
  bool sampleRateShading;

  // depth stencil
  bool depthEnable;
  bool stencilEnable;
  VkStencilOp stencilOp;

  // color blend
  bool colourOutput;
  bool blendEnable;
  VkBlendFactor srcBlend;
  VkBlendFactor dstBlend;
  uint32_t writeMask;
};

static void create(WrappedVulkan *driver, const char *objName, const int line, VkPipeline *pipe,
                   const ConciseGraphicsPipeline &info)
{
  // if the module didn't compile, this pipeline is not be supported. Silently don't create it, code
  // later should handle the missing pipeline as indicating lack of support
  if(info.vertex == VK_NULL_HANDLE || info.fragment == VK_NULL_HANDLE)
    return;

  // first configure the structs that contain parameters derived from the info parameter

  const VkPipelineShaderStageCreateInfo shaderStages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
       info.vertex, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
       info.fragment, "main", NULL},
  };

  const VkPipelineDynamicStateCreateInfo dynamicState = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      NULL,
      0,
      (uint32_t)info.dynstates.size(),
      info.dynstates.begin(),
  };

  VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
  };

  msaa.rasterizationSamples = info.sampleCount;
  if(info.sampleRateShading)
  {
    msaa.minSampleShading = 1.0f;
    msaa.sampleShadingEnable = true;
  }

  const VkPipelineDepthStencilStateCreateInfo depthStencil = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      NULL,
      0,
      info.depthEnable,
      info.depthEnable,
      VK_COMPARE_OP_ALWAYS,
      false,
      info.stencilEnable,
      {info.stencilOp, info.stencilOp, info.stencilOp, VK_COMPARE_OP_ALWAYS, 0xff, 0xff, 0},
      {info.stencilOp, info.stencilOp, info.stencilOp, VK_COMPARE_OP_ALWAYS, 0xff, 0xff, 0},
      0.0f,
      1.0f,
  };

  const VkPipelineColorBlendAttachmentState colAttach = {
      info.blendEnable,
      // colour blending
      info.srcBlend, info.dstBlend, VK_BLEND_OP_ADD,
      // alpha blending
      info.srcBlend, info.dstBlend, VK_BLEND_OP_ADD,
      // write mask
      info.writeMask,
  };

  const VkPipelineColorBlendStateCreateInfo colorBlend = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      VK_LOGIC_OP_NO_OP,
      info.colourOutput ? 1U : 0U,
      &colAttach,
      {1.0f, 1.0f, 1.0f, 1.0f},
  };

  // below this point, structs are not affected by the info

  const VkPipelineVertexInputStateCreateInfo vertexInput = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
  };

  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

  VkPipelineViewportStateCreateInfo viewScissor = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewScissor.viewportCount = viewScissor.scissorCount = 1;

  // add default scissor, if scissor is dynamic this will be ignored.
  VkRect2D scissor = {{0, 0}, {16384, 16384}};
  viewScissor.pScissors = &scissor;

  // can't really make a sensible one-size-fits-all default viewport like we can with scissors, so
  // make it small.
  VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  viewScissor.pViewports = &viewport;

  VkPipelineRasterizationStateCreateInfo raster = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
  };

  raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
  raster.lineWidth = 1.0f;

  const VkGraphicsPipelineCreateInfo graphicsPipeInfo = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      NULL,
      0,
      2,
      shaderStages,
      &vertexInput,
      &inputAssembly,
      NULL,    // tess
      &viewScissor,
      &raster,
      &msaa,
      &depthStencil,
      &colorBlend,
      &dynamicState,
      info.pipeLayout,
      info.renderPass,
      0,                 // sub pass
      VK_NULL_HANDLE,    // base pipeline handle
      -1,                // base pipeline index
  };

  VkResult vkr = driver->vkCreateGraphicsPipelines(
      driver->GetDev(), driver->GetShaderCache()->GetPipeCache(), 1, &graphicsPipeInfo, NULL, pipe);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object %s at line %i, vkr was %s", objName, line, ToStr(vkr).c_str());
}

// utility macro that lets us check for VkResult failures inside the utility helpers while
// preserving context from outside
#define CREATE_OBJECT(obj, ...) create(driver, #obj, __LINE__, &obj, __VA_ARGS__)

VulkanDebugManager::VulkanDebugManager(WrappedVulkan *driver)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(VulkanDebugManager));

  m_pDriver = driver;

  m_Device = m_pDriver->GetDev();
  VkDevice dev = m_Device;

  VulkanResourceManager *rm = driver->GetResourceManager();

  VkResult vkr = VK_SUCCESS;

  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  VkDescriptorPoolSize poolTypes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * ARRAY_COUNT(m_ArrayMSDescSet)},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * ARRAY_COUNT(m_ArrayMSDescSet)},
  };

  VkDescriptorPoolCreateInfo poolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      ARRAY_COUNT(m_ArrayMSDescSet),
      ARRAY_COUNT(poolTypes),
      &poolTypes[0],
  };

  CREATE_OBJECT(m_ArrayMSSampler, VK_FILTER_NEAREST);

  rm->SetInternalResource(GetResID(m_ArrayMSSampler));

  vkr = m_pDriver->vkCreateDescriptorPool(dev, &poolInfo, NULL, &m_ArrayMSDescriptorPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_ArrayMSDescriptorPool));

  CREATE_OBJECT(m_ArrayMSDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  rm->SetInternalResource(GetResID(m_ArrayMSDescSetLayout));

  CREATE_OBJECT(m_ArrayMSPipeLayout, m_ArrayMSDescSetLayout, sizeof(Vec4u));

  rm->SetInternalResource(GetResID(m_ArrayMSPipeLayout));

  //////////////////////////////////////////////////////////////////
  // Color MS to Array copy (via compute)

  CREATE_OBJECT(m_MS2ArrayPipe, m_ArrayMSPipeLayout,
                shaderCache->GetBuiltinModule(BuiltinShader::MS2ArrayCS));
  CREATE_OBJECT(m_Array2MSPipe, m_ArrayMSPipeLayout,
                shaderCache->GetBuiltinModule(BuiltinShader::Array2MSCS));

  rm->SetInternalResource(GetResID(m_MS2ArrayPipe));
  rm->SetInternalResource(GetResID(m_Array2MSPipe));

  //////////////////////////////////////////////////////////////////
  // Depth MS to Array copy (via graphics)

  // need a dummy UINT texture to fill the binding when we don't have a stencil aspect to copy.
  // unfortunately there's no single guaranteed UINT format that can be sampled as MSAA, so we try a
  // few since hopefully we'll find one that will work.
  VkFormat attemptFormats[] = {VK_FORMAT_R8G8B8A8_UINT,     VK_FORMAT_R8_UINT,
                               VK_FORMAT_S8_UINT,           VK_FORMAT_D32_SFLOAT_S8_UINT,
                               VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};

  for(VkFormat f : attemptFormats)
  {
    VkImageAspectFlags viewAspectMask =
        IsStencilFormat(f) ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageAspectFlags barrierAspectMask = viewAspectMask;

    if(IsDepthAndStencilFormat(f) && (barrierAspectMask & VK_IMAGE_ASPECT_STENCIL_BIT))
      barrierAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

    VkFormatProperties props = {};
    driver->vkGetPhysicalDeviceFormatProperties(driver->GetPhysDev(), f, &props);

    if(!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
      continue;

    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        f,
        {1, 1, 1},
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageFormatProperties imgprops = {};
    vkr = driver->vkGetPhysicalDeviceImageFormatProperties(driver->GetPhysDev(), f,
                                                           imInfo.imageType, imInfo.tiling,
                                                           imInfo.usage, imInfo.flags, &imgprops);

    if(vkr == VK_ERROR_FORMAT_NOT_SUPPORTED)
      continue;

    // if it doesn't support MSAA, bail out
    if(imgprops.sampleCounts == VK_SAMPLE_COUNT_1_BIT)
      continue;

    vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &m_DummyStencilImage[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    rm->SetInternalResource(GetResID(m_DummyStencilImage[0]));

    imInfo.samples = VK_SAMPLE_COUNT_2_BIT;

    // MoltenVK seems to only support 4/8 samples and not 2...
    if(imgprops.sampleCounts & VK_SAMPLE_COUNT_2_BIT)
      imInfo.samples = VK_SAMPLE_COUNT_2_BIT;
    else if(imgprops.sampleCounts & VK_SAMPLE_COUNT_4_BIT)
      imInfo.samples = VK_SAMPLE_COUNT_4_BIT;
    else if(imgprops.sampleCounts & VK_SAMPLE_COUNT_8_BIT)
      imInfo.samples = VK_SAMPLE_COUNT_8_BIT;
    else if(imgprops.sampleCounts & VK_SAMPLE_COUNT_16_BIT)
      imInfo.samples = VK_SAMPLE_COUNT_16_BIT;
    else if(imgprops.sampleCounts & VK_SAMPLE_COUNT_32_BIT)
      imInfo.samples = VK_SAMPLE_COUNT_32_BIT;
    else
      RDCWARN("Can't find supported MSAA sample count");

    RDCASSERT(imgprops.sampleCounts & imInfo.samples, imgprops.sampleCounts, imInfo.samples);

    vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &m_DummyStencilImage[1]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    rm->SetInternalResource(GetResID(m_DummyStencilImage[1]));

    VkMemoryRequirements mrq[2] = {};
    driver->vkGetImageMemoryRequirements(driver->GetDev(), m_DummyStencilImage[0], &mrq[0]);
    driver->vkGetImageMemoryRequirements(driver->GetDev(), m_DummyStencilImage[1], &mrq[1]);

    uint32_t memoryTypeBits = (mrq[0].memoryTypeBits & mrq[1].memoryTypeBits);

    // assume we have some memory type available in common
    RDCASSERT(memoryTypeBits, mrq[0].memoryTypeBits, mrq[1].memoryTypeBits);

    // allocate memory
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
        AlignUp(mrq[0].size, mrq[1].alignment) + mrq[1].size,
        driver->GetGPULocalMemoryIndex(memoryTypeBits),
    };

    vkr = driver->vkAllocateMemory(driver->GetDev(), &allocInfo, NULL, &m_DummyStencilMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    rm->SetInternalResource(GetResID(m_DummyStencilMemory));

    vkr =
        driver->vkBindImageMemory(driver->GetDev(), m_DummyStencilImage[0], m_DummyStencilMemory, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = driver->vkBindImageMemory(driver->GetDev(), m_DummyStencilImage[1], m_DummyStencilMemory,
                                    AlignUp(mrq[0].size, mrq[1].alignment));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        m_DummyStencilImage[0],
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        f,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {
            viewAspectMask, 0, 1, 0, 1,
        },
    };

    vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &m_DummyStencilView[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    rm->SetInternalResource(GetResID(m_DummyStencilView[0]));

    viewInfo.image = m_DummyStencilImage[1];

    vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &m_DummyStencilView[1]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    rm->SetInternalResource(GetResID(m_DummyStencilView[1]));

    VkCommandBuffer cmd = driver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // need to update image layout into valid state
    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_DummyStencilImage[0]),
        {barrierAspectMask, 0, 1, 0, 1},
    };

    DoPipelineBarrier(cmd, 1, &barrier);

    barrier.image = Unwrap(m_DummyStencilImage[1]);

    DoPipelineBarrier(cmd, 1, &barrier);

    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

    break;
  }

  if(m_DummyStencilImage[0] == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't find any integer format we could generate a dummy multisampled image with");
  }

  for(size_t i = 0; i < ARRAY_COUNT(m_ArrayMSDescSet); i++)
  {
    CREATE_OBJECT(m_ArrayMSDescSet[i], m_ArrayMSDescriptorPool, m_ArrayMSDescSetLayout);

    rm->SetInternalResource(GetResID(m_ArrayMSDescSet[i]));
  }

  VkFormat formats[] = {
      VK_FORMAT_D16_UNORM,         VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32,
      VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT,        VK_FORMAT_D32_SFLOAT_S8_UINT,
  };

  VkSampleCountFlagBits sampleCounts[] = {
      VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT,
  };

  RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthMS2ArrayPipe) == ARRAY_COUNT(formats),
                    "Array count mismatch");
  RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthArray2MSPipe) == ARRAY_COUNT(formats),
                    "Array count mismatch");
  RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthArray2MSPipe[0]) == ARRAY_COUNT(sampleCounts),
                    "Array count mismatch");

  // we use VK_IMAGE_LAYOUT_GENERAL here because it matches the expected layout for the
  // non-depth copy, which uses a storage image.
  VkImageLayout rpLayout = VK_IMAGE_LAYOUT_GENERAL;

  for(size_t f = 0; f < ARRAY_COUNT(formats); f++)
  {
    // if the format isn't supported at all, bail out and don't try to create anything
    if(!(m_pDriver->GetFormatProperties(formats[f]).optimalTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
      RDCDEBUG("Depth copies MSAA -> Array not supported for format %s", ToStr(formats[f]).c_str());
      continue;
    }

    VkRenderPass depthMS2ArrayRP = VK_NULL_HANDLE;

    CREATE_OBJECT(depthMS2ArrayRP, formats[f], VK_SAMPLE_COUNT_1_BIT, rpLayout);

    ConciseGraphicsPipeline depthPipeInfo = {
        depthMS2ArrayRP,
        m_ArrayMSPipeLayout,
        shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
        shaderCache->GetBuiltinModule(BuiltinShader::DepthMS2ArrayFS),
        {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_STENCIL_REFERENCE},
        VK_SAMPLE_COUNT_1_BIT,
        false,    // sampleRateShading
        true,     // depthEnable
        true,     // stencilEnable
        VK_STENCIL_OP_REPLACE,
        false,    // colourOutput
        false,    // blendEnable
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ZERO,
        0xf,    // writeMask
    };

    CREATE_OBJECT(m_DepthMS2ArrayPipe[f], depthPipeInfo);

    rm->SetInternalResource(GetResID(m_DepthMS2ArrayPipe[f]));

    m_pDriver->vkDestroyRenderPass(dev, depthMS2ArrayRP, NULL);

    if(!m_pDriver->GetDeviceEnabledFeatures().sampleRateShading)
    {
      RDCDEBUG("No depth Array -> MSAA copies can be supported without sample rate shading");
      continue;
    }

    for(size_t s = 0; s < ARRAY_COUNT(sampleCounts); s++)
    {
      // if this sample count isn't supported, don't create it
      if(!(m_pDriver->GetDeviceProps().limits.framebufferDepthSampleCounts &
           (uint32_t)sampleCounts[s]))
      {
        RDCDEBUG("Depth copies Array -> MSAA not supported for sample count %u on format %s",
                 sampleCounts[s], ToStr(formats[f]).c_str());
        continue;
      }

      VkRenderPass depthArray2MSRP;

      CREATE_OBJECT(depthArray2MSRP, formats[f], sampleCounts[s], rpLayout);

      depthPipeInfo.fragment = shaderCache->GetBuiltinModule(BuiltinShader::DepthArray2MSFS);
      depthPipeInfo.renderPass = depthArray2MSRP;
      depthPipeInfo.sampleCount = sampleCounts[s];
      depthPipeInfo.sampleRateShading = true;

      CREATE_OBJECT(m_DepthArray2MSPipe[f][s], depthPipeInfo);

      rm->SetInternalResource(GetResID(m_DepthArray2MSPipe[f][s]));

      m_pDriver->vkDestroyRenderPass(dev, depthArray2MSRP, NULL);
    }
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    VkDescriptorPoolSize descPoolTypes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ARRAY_COUNT(m_DiscardSet)},
    };

    VkDescriptorPoolCreateInfo descPoolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        NULL,
        0,
        ARRAY_COUNT(m_DiscardSet),
        ARRAY_COUNT(descPoolTypes),
        &descPoolTypes[0],
    };

    // create descriptor pool
    vkr = driver->vkCreateDescriptorPool(driver->GetDev(), &descPoolInfo, NULL, &m_DiscardPool);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    CREATE_OBJECT(m_DiscardSetLayout,
                  {
                      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                  });

    CREATE_OBJECT(m_DiscardLayout, m_DiscardSetLayout, 4);

    ResourceFormat fmt;
    fmt.type = ResourceFormatType::Regular;
    fmt.compType = CompType::Float;
    fmt.compByteWidth = 4;
    fmt.compCount = 1;

    for(size_t i = 0; i < ARRAY_COUNT(m_DiscardSet); i++)
    {
      CREATE_OBJECT(m_DiscardSet[i], m_DiscardPool, m_DiscardSetLayout);

      bytebuf pattern = GetDiscardPattern(DiscardType(i), fmt);

      m_DiscardCB[i].Create(m_pDriver, m_Device, pattern.size(), 1, 0);

      memcpy(m_DiscardCB[i].Map(), pattern.data(), pattern.size());
      m_DiscardCB[i].Unmap();

      VkDescriptorBufferInfo bufInfo = {};
      m_DiscardCB[i].FillDescriptor(bufInfo);

      VkWriteDescriptorSet writes[] = {
          {
              VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_DiscardSet[i]), 0, 0, 1,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufInfo, NULL,
          },
      };

      ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writes), writes, 0, NULL);
    }
  }

  // we only need this during replay, so don't create otherwise.
  if(RenderDoc::Inst().IsReplayApp())
  {
    m_ReadbackWindow.Create(driver, dev, STAGE_BUFFER_BYTE_SIZE, 1, GPUBuffer::eGPUBufferReadback);
  }
  else
  {
    m_ReadbackWindow.Create(driver, dev, 256 * 1024 * 1024ULL, 1, GPUBuffer::eGPUBufferReadback);

    vkr = ObjDisp(dev)->MapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem), 0, VK_WHOLE_SIZE, 0,
                                  (void **)&m_ReadbackPtr);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
}

VulkanDebugManager::~VulkanDebugManager()
{
  VkDevice dev = m_Device;

  m_Custom.Destroy(m_pDriver);

  m_ReadbackWindow.Destroy();

  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(uint32_t i = 0; i < VKMeshDisplayPipelines::ePipe_Count; i++)
      m_pDriver->vkDestroyPipeline(dev, it->second.pipes[i], NULL);

  m_pDriver->vkDestroyDescriptorPool(dev, m_ArrayMSDescriptorPool, NULL);
  m_pDriver->vkDestroySampler(dev, m_ArrayMSSampler, NULL);

  m_pDriver->vkDestroyImageView(dev, m_DummyStencilView[0], NULL);
  m_pDriver->vkDestroyImageView(dev, m_DummyStencilView[1], NULL);
  m_pDriver->vkDestroyImage(dev, m_DummyStencilImage[0], NULL);
  m_pDriver->vkDestroyImage(dev, m_DummyStencilImage[1], NULL);
  m_pDriver->vkFreeMemory(dev, m_DummyStencilMemory, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_ArrayMSDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_ArrayMSPipeLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_Array2MSPipe, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_MS2ArrayPipe, NULL);

  m_pDriver->vkDestroyDescriptorPool(dev, m_DiscardPool, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_DiscardLayout, NULL);
  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_DiscardSetLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_DiscardCB); i++)
    m_DiscardCB[i].Destroy();

  for(auto it = m_DiscardImages.begin(); it != m_DiscardImages.end(); it++)
  {
    for(VkImageView view : it->second.views)
      m_pDriver->vkDestroyImageView(dev, view, NULL);
    for(VkFramebuffer fb : it->second.fbs)
      m_pDriver->vkDestroyFramebuffer(dev, fb, NULL);
  }

  for(auto it = m_DiscardPipes.begin(); it != m_DiscardPipes.end(); it++)
  {
    m_pDriver->vkDestroyPipeline(dev, it->second.pso, NULL);
    m_pDriver->vkDestroyRenderPass(dev, it->second.rp, NULL);
  }

  for(auto it = m_DiscardPatterns.begin(); it != m_DiscardPatterns.end(); it++)
    m_pDriver->vkDestroyBuffer(dev, it->second, NULL);

  for(size_t i = 0; i < ARRAY_COUNT(m_DepthMS2ArrayPipe); i++)
    m_pDriver->vkDestroyPipeline(dev, m_DepthMS2ArrayPipe[i], NULL);

  for(size_t f = 0; f < ARRAY_COUNT(m_DepthArray2MSPipe); f++)
    for(size_t s = 0; s < ARRAY_COUNT(m_DepthArray2MSPipe[0]); s++)
      m_pDriver->vkDestroyPipeline(dev, m_DepthArray2MSPipe[f][s], NULL);
}

void VulkanDebugManager::CreateCustomShaderTex(uint32_t width, uint32_t height, uint32_t mip)
{
  WrappedVulkan *driver = m_pDriver;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  if(m_Custom.TexImg != VK_NULL_HANDLE)
  {
    if(width == m_Custom.TexWidth && height == m_Custom.TexHeight)
    {
      // recreate framebuffer for this mip

      // Create framebuffer rendering just to overlay image, no depth
      VkFramebufferCreateInfo fbinfo = {
          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          NULL,
          0,
          m_Custom.TexRP,
          1,
          &m_Custom.TexImgView[mip],
          RDCMAX(1U, width >> mip),
          RDCMAX(1U, height >> mip),
          1,
      };

      vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_Custom.TexFB);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
      return;
    }

    m_pDriver->vkDestroyRenderPass(dev, m_Custom.TexRP, NULL);
    m_pDriver->vkDestroyFramebuffer(dev, m_Custom.TexFB, NULL);
    for(size_t i = 0; i < ARRAY_COUNT(m_Custom.TexImgView); i++)
      m_pDriver->vkDestroyImageView(dev, m_Custom.TexImgView[i], NULL);
    RDCEraseEl(m_Custom.TexImgView);
    m_pDriver->vkDestroyImage(dev, m_Custom.TexImg, NULL);
  }

  m_Custom.TexWidth = width;
  m_Custom.TexHeight = height;

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

  vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, NULL, &m_Custom.TexImg);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(m_Device, m_Custom.TexImg, &mrq);

  // if no memory is allocated, or it's not enough,
  // then allocate
  if(m_Custom.TexMem == VK_NULL_HANDLE || mrq.size > m_Custom.TexMemSize)
  {
    if(m_Custom.TexMem != VK_NULL_HANDLE)
      m_pDriver->vkFreeMemory(m_Device, m_Custom.TexMem, NULL);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &m_Custom.TexMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_Custom.TexMemSize = mrq.size;
  }

  vkr = m_pDriver->vkBindImageMemory(m_Device, m_Custom.TexImg, m_Custom.TexMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      m_Custom.TexImg,
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
    vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &m_Custom.TexImgView[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  // need to update image layout into valid state

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  m_pDriver->FindImageState(GetResID(m_Custom.TexImg))
      ->InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         m_pDriver->GetImageTransitionInfo());

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  CREATE_OBJECT(m_Custom.TexRP, imInfo.format, imInfo.samples);

  // Create framebuffer rendering just to overlay image, no depth
  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      NULL,
      0,
      m_Custom.TexRP,
      1,
      &m_Custom.TexImgView[mip],
      RDCMAX(1U, width >> mip),
      RDCMAX(1U, height >> mip),
      1,
  };

  vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_Custom.TexFB);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanDebugManager::CreateCustomShaderPipeline(ResourceId shader, VkPipelineLayout pipeLayout)
{
  WrappedVulkan *driver = m_pDriver;

  if(shader == ResourceId())
    return;

  if(m_Custom.TexPipeline != VK_NULL_HANDLE)
  {
    if(m_Custom.TexShader == shader)
      return;

    m_pDriver->vkDestroyPipeline(m_Device, m_Custom.TexPipeline, NULL);
  }

  m_Custom.TexShader = shader;

  ConciseGraphicsPipeline customPipe = {
      m_Custom.TexRP,
      pipeLayout,
      m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::BlitVS),
      m_pDriver->GetResourceManager()->GetCurrentHandle<VkShaderModule>(shader),
      {VK_DYNAMIC_STATE_VIEWPORT},
      VK_SAMPLE_COUNT_1_BIT,
      false,    // sampleRateShading
      false,    // depthEnable
      false,    // stencilEnable
      VK_STENCIL_OP_KEEP,
      true,     // colourOutput
      false,    // blendEnable
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      0xf,    // writeMask
  };

  CREATE_OBJECT(m_Custom.TexPipeline, customPipe);
}

// TODO: Point meshes don't pick correctly
uint32_t VulkanReplay::PickVertex(uint32_t eventId, int32_t w, int32_t h, const MeshDisplay &cfg,
                                  uint32_t x, uint32_t y)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  VkMarkerRegion::Begin(StringFormat::Fmt("VulkanReplay::PickVertex(%u, %u)", x, y));

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(w) / float(h));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f pickMVP = projMat.Mul(camMat);

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

  const bool fandecode =
      (cfg.position.topology == Topology::TriangleFan && cfg.position.allowRestart);

  uint32_t numIndices = cfg.position.numIndices;

  bytebuf idxs;

  uint32_t minIndex = 0;
  uint32_t maxIndex = cfg.position.numIndices;

  if(cfg.position.indexByteStride && cfg.position.indexResourceId != ResourceId())
    GetBufferData(cfg.position.indexResourceId, cfg.position.indexByteOffset, 0, idxs);

  uint32_t idxclamp = 0;
  if(cfg.position.baseVertex < 0)
    idxclamp = uint32_t(-cfg.position.baseVertex);

  // We copy into our own buffers to promote to the target type (uint32) that the shader expects.
  // Most IBs will be 16-bit indices, most VBs will not be float4. We also apply baseVertex here

  if(!idxs.empty())
  {
    rdcarray<uint32_t> idxtmp;

    // if it's a triangle fan that allows restart, we'll have to unpack it.
    // Allocate enough space for the list on the GPU, and enough temporary space to upcast into
    // first
    if(fandecode)
    {
      idxtmp.resize(numIndices);

      numIndices *= 3;
    }

    // resize up on demand
    if(m_VertexPick.IBSize < numIndices * sizeof(uint32_t))
    {
      if(m_VertexPick.IBSize > 0)
      {
        m_VertexPick.IB.Destroy();
        m_VertexPick.IBUpload.Destroy();
      }

      m_VertexPick.IBSize = numIndices * sizeof(uint32_t);

      m_VertexPick.IB.Create(m_pDriver, dev, m_VertexPick.IBSize, 1,
                             GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
      m_VertexPick.IBUpload.Create(m_pDriver, dev, m_VertexPick.IBSize, 1, 0);
    }

    uint32_t *outidxs = (uint32_t *)m_VertexPick.IBUpload.Map();
    uint32_t *mappedPtr = outidxs;

    memset(outidxs, 0, m_VertexPick.IBSize);

    // if we're decoding a fan, we write into our temporary vector first
    if(fandecode)
      outidxs = idxtmp.data();

    uint16_t *idxs16 = (uint16_t *)&idxs[0];
    uint32_t *idxs32 = (uint32_t *)&idxs[0];

    size_t idxcount = 0;

    if(cfg.position.indexByteStride == 2)
    {
      size_t bufsize = idxs.size() / 2;

      for(uint32_t i = 0; i < bufsize && i < cfg.position.numIndices; i++)
      {
        uint32_t idx = idxs16[i];

        if(idx < idxclamp)
          idx = 0;
        else if(cfg.position.baseVertex < 0)
          idx -= idxclamp;
        else if(cfg.position.baseVertex > 0)
          idx += cfg.position.baseVertex;

        if(i == 0)
        {
          minIndex = maxIndex = idx;
        }
        else
        {
          minIndex = RDCMIN(idx, minIndex);
          maxIndex = RDCMAX(idx, maxIndex);
        }

        outidxs[i] = idx;
        idxcount++;
      }
    }
    else
    {
      uint32_t bufsize = uint32_t(idxs.size() / 4);

      minIndex = maxIndex = idxs32[0];

      for(uint32_t i = 0; i < RDCMIN(bufsize, cfg.position.numIndices); i++)
      {
        uint32_t idx = idxs32[i];

        if(idx < idxclamp)
          idx = 0;
        else if(cfg.position.baseVertex < 0)
          idx -= idxclamp;
        else if(cfg.position.baseVertex > 0)
          idx += cfg.position.baseVertex;

        minIndex = RDCMIN(idx, minIndex);
        maxIndex = RDCMAX(idx, maxIndex);

        outidxs[i] = idx;
        idxcount++;
      }
    }

    // if it's a triangle fan that allows restart, unpack it
    if(cfg.position.topology == Topology::TriangleFan && cfg.position.allowRestart)
    {
      // resize to how many indices were actually read
      idxtmp.resize(idxcount);

      // patch the index buffer
      PatchTriangleFanRestartIndexBufer(idxtmp, cfg.position.restartIndex);

      for(uint32_t &idx : idxtmp)
      {
        if(idx == cfg.position.restartIndex)
          idx = 0;
      }

      numIndices = (uint32_t)idxtmp.size();

      // now copy the decoded list to the GPU
      memcpy(mappedPtr, idxtmp.data(), idxtmp.size() * sizeof(uint32_t));
    }

    m_VertexPick.IBUpload.Unmap();
  }

  // unpack and linearise the data
  {
    bytebuf oldData;
    GetBufferData(cfg.position.vertexResourceId, cfg.position.vertexByteOffset, 0, oldData);

    // clamp maxIndex to upper bound in case we got invalid indices or primitive restart indices
    maxIndex = RDCMIN(maxIndex, uint32_t(oldData.size() / RDCMAX(1U, cfg.position.vertexByteStride)));

    if(m_VertexPick.VBSize < (maxIndex + 1) * sizeof(FloatVector))
    {
      if(m_VertexPick.VBSize > 0)
      {
        m_VertexPick.VB.Destroy();
        m_VertexPick.VBUpload.Destroy();
      }

      m_VertexPick.VBSize = (maxIndex + 1) * sizeof(FloatVector);

      m_VertexPick.VB.Create(m_pDriver, dev, m_VertexPick.VBSize, 1,
                             GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
      m_VertexPick.VBUpload.Create(m_pDriver, dev, m_VertexPick.VBSize, 1, 0);
    }

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

    FloatVector *vbData = (FloatVector *)m_VertexPick.VBUpload.Map();

    // the index buffer may refer to vertices past the start of the vertex buffer, so we can't just
    // conver the first N vertices we'll need.
    // Instead we grab min and max above, and convert every vertex in that range. This might
    // slightly over-estimate but not as bad as 0-max or the whole buffer.
    for(uint32_t idx = minIndex; idx <= maxIndex; idx++)
      vbData[idx] = HighlightCache::InterpretVertex(data, idx, cfg.position.vertexByteStride,
                                                    cfg.position.format, dataEnd, valid);

    m_VertexPick.VBUpload.Unmap();
  }

  MeshPickUBOData *ubo = (MeshPickUBOData *)m_VertexPick.UBO.Map();

  ubo->rayPos = rayPos;
  ubo->rayDir = rayDir;
  ubo->use_indices = cfg.position.indexByteStride ? 1U : 0U;
  ubo->numVerts = numIndices;
  bool isTriangleMesh = true;

  switch(cfg.position.topology)
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
      if(fandecode)
        ubo->meshMode = MESH_TRIANGLE_LIST;
      else
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

  m_VertexPick.UBO.Unmap();

  VkDescriptorBufferInfo ibInfo = {};
  VkDescriptorBufferInfo vbInfo = {};

  m_VertexPick.VB.FillDescriptor(vbInfo);
  m_VertexPick.IB.FillDescriptor(ibInfo);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_VertexPick.DescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &vbInfo, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_VertexPick.DescSet), 2, 0, 1,
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
  vt->CmdFillBuffer(Unwrap(cmd), Unwrap(m_VertexPick.Result.buf), 0, sizeof(uint32_t) * 4, 0);

  VkBufferMemoryBarrier bufBarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(m_VertexPick.Result.buf),
      0,
      VK_WHOLE_SIZE,
  };

  // wait for zero to be written to atomic counter before using in shader
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  // copy uploaded VB and if needed IB
  if(!idxs.empty())
  {
    // wait for writes
    bufBarrier.buffer = Unwrap(m_VertexPick.IBUpload.buf);
    bufBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    DoPipelineBarrier(cmd, 1, &bufBarrier);

    // do copy
    bufCopy.size = m_VertexPick.IBSize;
    vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_VertexPick.IBUpload.buf), Unwrap(m_VertexPick.IB.buf),
                      1, &bufCopy);

    // wait for copy
    bufBarrier.buffer = Unwrap(m_VertexPick.IB.buf);
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
    DoPipelineBarrier(cmd, 1, &bufBarrier);
  }

  // wait for writes
  bufBarrier.buffer = Unwrap(m_VertexPick.VBUpload.buf);
  bufBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  // do copy
  bufCopy.size = m_VertexPick.VBSize;
  vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_VertexPick.VBUpload.buf), Unwrap(m_VertexPick.VB.buf), 1,
                    &bufCopy);

  // wait for copy
  bufBarrier.buffer = Unwrap(m_VertexPick.VB.buf);
  bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_VertexPick.Pipeline));
  vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_VertexPick.Layout),
                            0, 1, UnwrapPtr(m_VertexPick.DescSet), 0, NULL);

  uint32_t workgroupx = uint32_t(cfg.position.numIndices / 128 + 1);
  vt->CmdDispatch(Unwrap(cmd), workgroupx, 1, 1);

  // wait for shader to finish writing before transferring to readback buffer
  bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bufBarrier.buffer = Unwrap(m_VertexPick.Result.buf);
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  bufCopy.size = m_VertexPick.Result.totalsize;

  // copy to readback buffer
  vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_VertexPick.Result.buf),
                    Unwrap(m_VertexPick.ResultReadback.buf), 1, &bufCopy);

  // wait for transfer to finish before reading on CPU
  bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  bufBarrier.buffer = Unwrap(m_VertexPick.ResultReadback.buf);
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  VkResult vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  uint32_t *pickResultData = (uint32_t *)m_VertexPick.ResultReadback.Map();
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
      for(uint32_t i = 1; i < RDCMIN((uint32_t)VertexPicking::MaxMeshPicks, numResults); i++)
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
      for(uint32_t i = 1; i < RDCMIN((uint32_t)VertexPicking::MaxMeshPicks, numResults); i++)
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

  m_VertexPick.ResultReadback.Unmap();

  VkMarkerRegion::Set(StringFormat::Fmt("Result is %u", ret));

  VkMarkerRegion::End();

  if(fandecode)
  {
    // undo the triangle list expansion
    if(ret > 2)
      ret = (ret + 3) / 3 + 1;
  }

  return ret;
}

const VulkanCreationInfo::Image &VulkanDebugManager::GetImageInfo(ResourceId img) const
{
  auto it = m_pDriver->m_CreationInfo.m_Image.find(img);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_Image.end());
  return it->second;
}

const VulkanCreationInfo::ImageView &VulkanDebugManager::GetImageViewInfo(ResourceId imgView) const
{
  auto it = m_pDriver->m_CreationInfo.m_ImageView.find(imgView);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_ImageView.end());
  return it->second;
}

const VulkanCreationInfo::Pipeline &VulkanDebugManager::GetPipelineInfo(ResourceId pipe) const
{
  auto it = m_pDriver->m_CreationInfo.m_Pipeline.find(pipe);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_Pipeline.end());
  return it->second;
}

const VulkanCreationInfo::ShaderModule &VulkanDebugManager::GetShaderInfo(ResourceId shader) const
{
  auto it = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_ShaderModule.end());
  return it->second;
}

const VulkanCreationInfo::Framebuffer &VulkanDebugManager::GetFramebufferInfo(ResourceId fb) const
{
  auto it = m_pDriver->m_CreationInfo.m_Framebuffer.find(fb);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_Framebuffer.end());
  return it->second;
}

const VulkanCreationInfo::RenderPass &VulkanDebugManager::GetRenderPassInfo(ResourceId rp) const
{
  auto it = m_pDriver->m_CreationInfo.m_RenderPass.find(rp);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_RenderPass.end());
  return it->second;
}

const VulkanCreationInfo::PipelineLayout &VulkanDebugManager::GetPipelineLayoutInfo(ResourceId rp) const
{
  auto it = m_pDriver->m_CreationInfo.m_PipelineLayout.find(rp);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_PipelineLayout.end());
  return it->second;
}

const DescSetLayout &VulkanDebugManager::GetDescSetLayout(ResourceId dsl) const
{
  auto it = m_pDriver->m_CreationInfo.m_DescSetLayout.find(dsl);
  RDCASSERT(it != m_pDriver->m_CreationInfo.m_DescSetLayout.end());
  return it->second;
}

const WrappedVulkan::DescriptorSetInfo &VulkanDebugManager::GetDescSetInfo(ResourceId ds) const
{
  auto it = m_pDriver->m_DescriptorSetState.find(ds);
  RDCASSERT(it != m_pDriver->m_DescriptorSetState.end());
  return it->second;
}

void VulkanDebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  WrappedVkRes *res = m_pDriver->GetResourceManager()->GetCurrentResource(buff);

  if(res == VK_NULL_HANDLE)
  {
    RDCERR("Getting buffer data for unknown buffer/memory %s!", ToStr(buff).c_str());
    return;
  }

  VkBuffer srcBuf = VK_NULL_HANDLE;
  uint64_t bufsize = 0;

  if(WrappedVkDeviceMemory::IsAlloc(res))
  {
    srcBuf = m_pDriver->m_CreationInfo.m_Memory[buff].wholeMemBuf;
    bufsize = m_pDriver->m_CreationInfo.m_Memory[buff].size;

    if(srcBuf == VK_NULL_HANDLE)
    {
      RDCLOG(
          "Memory doesn't have wholeMemBuf, either non-buffer accessible (non-linear) or dedicated "
          "image memory");
      return;
    }
  }
  else
  {
    srcBuf = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(buff);
    bufsize = m_pDriver->m_CreationInfo.m_Buffer[buff].size;
  }

  if(offset >= bufsize)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(len == 0 || len > bufsize)
  {
    len = bufsize - offset;
  }

  if(VkDeviceSize(offset + len) > bufsize)
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

    cmd = m_pDriver->GetNextCmd();

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

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, Unwrap(m_ReadbackWindow.mem), 0, VK_WHOLE_SIZE,
    };

    vkr = vt->InvalidateMappedMemoryRanges(Unwrap(dev), 1, &range);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    RDCASSERT(pData != NULL);
    memcpy(&ret[dstoffset], pData, (size_t)chunkSize);

    srcoffset += chunkSize;
    dstoffset += (size_t)chunkSize;
    sizeRemaining -= chunkSize;

    vt->UnmapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem));
  }

  vt->DeviceWaitIdle(Unwrap(dev));
}

void VulkanDebugManager::FillWithDiscardPattern(VkCommandBuffer cmd, DiscardType type,
                                                VkImage image, VkImageLayout curLayout,
                                                VkImageSubresourceRange discardRange,
                                                VkRect2D discardRect)
{
  VkDevice dev = m_Device;
  const VkDevDispatchTable *vt = ObjDisp(dev);
  const VulkanCreationInfo::Image &imInfo = GetImageInfo(GetResID(image));

  VkMarkerRegion marker(
      cmd, StringFormat::Fmt("FillWithDiscardPattern %s", ToStr(GetResID(image)).c_str()));

  if(imInfo.samples > 1)
  {
    WrappedVulkan *driver = m_pDriver;

    bool depth = false;
    if(IsDepthOrStencilFormat(imInfo.format))
      depth = true;

    VkImageAspectFlags imAspects = FormatImageAspects(imInfo.format);

    rdcpair<VkFormat, VkSampleCountFlagBits> key = {imInfo.format, imInfo.samples};

    DiscardPassData &passdata = m_DiscardPipes[key];

    // create and cache a pipeline and RP that writes to this format and sample count
    if(passdata.pso == VK_NULL_HANDLE)
    {
      VkAttachmentReference attRef = {
          0, depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };

      VkAttachmentDescription attDesc = {
          0,
          imInfo.format,
          imInfo.samples,
          VK_ATTACHMENT_LOAD_OP_LOAD,
          VK_ATTACHMENT_STORE_OP_STORE,
          VK_ATTACHMENT_LOAD_OP_LOAD,
          VK_ATTACHMENT_STORE_OP_STORE,
          attRef.layout,
          attRef.layout,
      };

      VkSubpassDescription sub = {
          0, VK_PIPELINE_BIND_POINT_GRAPHICS,
      };

      if(depth)
      {
        sub.pDepthStencilAttachment = &attRef;
      }
      else
      {
        sub.pColorAttachments = &attRef;
        sub.colorAttachmentCount = 1;
      }

      VkRenderPassCreateInfo rpinfo = {
          VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &attDesc, 1, &sub, 0, NULL,
      };

      VkResult vkr = m_pDriver->vkCreateRenderPass(m_pDriver->GetDev(), &rpinfo, NULL, &passdata.rp);
      if(vkr != VK_SUCCESS)
        RDCERR("Failed to create shader debug render pass: %s", ToStr(vkr).c_str());

      ConciseGraphicsPipeline pipeInfo = {
          passdata.rp,
          m_DiscardLayout,
          m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::BlitVS),
          m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::DiscardFS),
          {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
          imInfo.samples,
          false,    // sampleRateShading
          true,     // depthEnable
          true,     // stencilEnable
          VK_STENCIL_OP_REPLACE,
          true,     // colourOutput
          false,    // blendEnable
          VK_BLEND_FACTOR_ONE,
          VK_BLEND_FACTOR_ZERO,
          0xf,    // writeMask
      };

      CREATE_OBJECT(passdata.pso, pipeInfo);
    }

    if(passdata.pso == VK_NULL_HANDLE)
      return;

    DiscardImgData &imgdata = m_DiscardImages[GetResID(image)];

    // create and cache views and framebuffers for every slice in this image
    if(imgdata.fbs.empty())
    {
      VkImageAspectFlags aspectMask = imAspects;

      for(int pass = 0; pass < 3; pass++)
      {
        // only depth/stencil images need multiple sets of views to mask out one aspect or the other
        if(pass > 0)
        {
          if(imAspects != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            break;

          if(pass == 1)
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
          else if(pass == 2)
            aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        for(uint32_t a = 0; a < imInfo.arrayLayers; a++)
        {
          VkImageViewCreateInfo viewInfo = {
              VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
              NULL,
              0,
              image,
              VK_IMAGE_VIEW_TYPE_2D,
              imInfo.format,
              {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
              {
                  aspectMask, 0, 1, a, 1,
              },
          };

          VkImageView view;
          VkResult vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &view);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          imgdata.views.push_back(view);

          // create framebuffer
          VkFramebufferCreateInfo fbinfo = {
              VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              NULL,
              0,
              passdata.rp,
              1,
              &view,
              imInfo.extent.width,
              imInfo.extent.height,
              1,
          };

          VkFramebuffer fb;
          vkr = driver->vkCreateFramebuffer(driver->GetDev(), &fbinfo, NULL, &fb);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          imgdata.fbs.push_back(fb);
        }
      }
    }

    if(imgdata.fbs.empty())
      return;

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(passdata.pso));
    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Unwrap(m_DiscardLayout), 0, 1,
                                        UnwrapPtr(m_DiscardSet[(size_t)type]), 0, NULL);
    VkViewport viewport = {0.0f, 0.0f, (float)imInfo.extent.width, (float)imInfo.extent.height, 1.0f};
    ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, 1U, &viewport);
    ObjDisp(cmd)->CmdSetScissor(Unwrap(cmd), 0, 1U, &discardRect);

    discardRect.extent.width =
        RDCMIN(discardRect.extent.width, imInfo.extent.width - discardRect.offset.x);
    discardRect.extent.height =
        RDCMIN(discardRect.extent.height, imInfo.extent.height - discardRect.offset.y);

    discardRange.layerCount =
        RDCMIN(discardRange.layerCount, imInfo.arrayLayers - discardRange.baseArrayLayer);

    VkRenderPassBeginInfo rpbegin = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        NULL,
        Unwrap(passdata.rp),
        VK_NULL_HANDLE,
        discardRect,
    };

    uint32_t pass = 0;

    VkImageMemoryBarrier dstimBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
        VK_ACCESS_ALL_READ_BITS | VK_ACCESS_ALL_WRITE_BITS, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        curLayout, depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                         : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, Unwrap(image), discardRange,
    };

    DoPipelineBarrier(cmd, 1, &dstimBarrier);

    ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DiscardLayout), VK_SHADER_STAGE_ALL, 0, 4,
                                   &pass);

    uint32_t offset = 0;
    if(imAspects != discardRange.aspectMask)
    {
      // if we're only discarding one of depth or stencil in a depth/stencil image, pick a
      // framebuffer that only targets that aspect.
      if(discardRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
        offset = imInfo.arrayLayers;
      else
        offset = imInfo.arrayLayers * 2;
    }

    for(uint32_t slice = discardRange.baseArrayLayer;
        slice < discardRange.baseArrayLayer + discardRange.layerCount; slice++)
    {
      rpbegin.framebuffer = Unwrap(imgdata.fbs[slice + offset]);
      ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      if(depth && discardRange.aspectMask != VK_IMAGE_ASPECT_DEPTH_BIT)
      {
        pass = 1;
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DiscardLayout), VK_SHADER_STAGE_ALL, 0,
                                       4, &pass);
        ObjDisp(cmd)->CmdSetStencilReference(
            Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT | VK_STENCIL_FACE_BACK_BIT, 0x00);
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

        pass = 2;
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DiscardLayout), VK_SHADER_STAGE_ALL, 0,
                                       4, &pass);
        ObjDisp(cmd)->CmdSetStencilReference(
            Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT | VK_STENCIL_FACE_BACK_BIT, 0xff);
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }
      else
      {
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }

      ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
    }

    dstimBarrier.oldLayout = dstimBarrier.newLayout;
    dstimBarrier.newLayout = curLayout;
    dstimBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dstimBarrier.dstAccessMask = VK_ACCESS_ALL_WRITE_BITS | VK_ACCESS_ALL_READ_BITS;

    DoPipelineBarrier(cmd, 1, &dstimBarrier);

    /*
    for(UINT sub = 0; sub < region->NumSubresources; sub++)
    {
      UINT subresource = region->FirstSubresource + sub;
      if(depth)
      {
        dsvDesc.Texture2DMSArray.FirstArraySlice = GetSliceForSubresource(res, subresource);
        m_pDevice->CreateDepthStencilView(res, &dsvDesc, dsv);
        cmd->OMSetRenderTargets(0, NULL, FALSE, &dsv);
      }
      else
      {
        rtvDesc.Texture2DMSArray.FirstArraySlice = GetSliceForSubresource(res, subresource);
        m_pDevice->CreateRenderTargetView(res, &rtvDesc, rtv);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, NULL);
      }

      UINT mip = GetMipForSubresource(res, subresource);
      UINT plane = GetPlaneForSubresource(res, subresource);

      for(D3D12_RECT r : rects)
      {
        r.right = RDCMIN(LONG(RDCMAX(1U, (UINT)desc.Width >> mip)), r.right);
        r.bottom = RDCMIN(LONG(RDCMAX(1U, (UINT)desc.Height >> mip)), r.bottom);

        cmd->RSSetScissorRects(1, &r);

      }
    }
    */

    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);

    return;
  }

  rdcpair<VkFormat, DiscardType> key = {imInfo.format, type};

  if(key.first == VK_FORMAT_S8_UINT)
    key.first = VK_FORMAT_D32_SFLOAT_S8_UINT;

  VkBuffer buf = m_DiscardPatterns[key];
  VkResult vkr = VK_SUCCESS;

  if(buf == VK_NULL_HANDLE)
  {
    bytebuf pattern = GetDiscardPattern(key.second, MakeResourceFormat(key.first));

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        pattern.size(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &buf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    MemoryAllocation alloc = m_pDriver->AllocateMemoryForResource(
        buf, MemoryScope::ImmutableReplayDebug, MemoryType::GPULocal);

    vkr = vt->BindBufferMemory(Unwrap(dev), Unwrap(buf), Unwrap(alloc.mem), alloc.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vt->CmdUpdateBuffer(Unwrap(cmd), Unwrap(buf), 0, pattern.size(), pattern.data());

    m_DiscardPatterns[key] = buf;

    VkBufferMemoryBarrier bufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(buf),
        0,
        VK_WHOLE_SIZE,
    };

    DoPipelineBarrier(cmd, 1, &bufBarrier);
  }

  VkImageAspectFlags aspectFlags = discardRange.aspectMask & FormatImageAspects(imInfo.format);

  rdcarray<VkBufferImageCopy> mainCopies, stencilCopies;

  VkExtent3D extent;

  // copy each slice/mip individually
  for(uint32_t a = 0; a < imInfo.arrayLayers; a++)
  {
    if(a < discardRange.baseArrayLayer || a >= discardRange.baseArrayLayer + discardRange.layerCount)
      continue;

    extent = imInfo.extent;
    extent.width = RDCMIN(extent.width, discardRect.offset.x + discardRect.extent.width);
    extent.height = RDCMIN(extent.height, discardRect.offset.y + discardRect.extent.height);

    for(uint32_t m = 0; m < imInfo.mipLevels; m++)
    {
      if(m >= discardRange.baseMipLevel && m < discardRange.baseMipLevel + discardRange.levelCount)
      {
        for(uint32_t z = 0; z < extent.depth; z++)
        {
          for(uint32_t y = discardRect.offset.y; y < extent.height; y += DiscardPatternHeight)
          {
            for(uint32_t x = discardRect.offset.x; x < extent.width; x += DiscardPatternWidth)
            {
              VkBufferImageCopy region = {
                  0,
                  0,
                  0,
                  {aspectFlags, m, a, 1},
                  {
                      (int)x, (int)y, (int)z,
                  },
              };

              region.imageExtent.width = RDCMIN(DiscardPatternWidth, extent.width - x);
              region.imageExtent.height = RDCMIN(DiscardPatternHeight, extent.height - y);
              region.imageExtent.depth = 1;

              region.bufferRowLength = DiscardPatternWidth;

              // for depth/stencil copies, write depth first
              if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

              if(aspectFlags != VK_IMAGE_ASPECT_STENCIL_BIT)
                mainCopies.push_back(region);

              if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
              {
                uint32_t depthStride = (imInfo.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2 : 4);
                // if it's a depth/stencil format, write stencil separately
                region.bufferOffset = DiscardPatternWidth * DiscardPatternHeight * depthStride;
                region.bufferRowLength = DiscardPatternWidth * depthStride;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

                stencilCopies.push_back(region);
              }
            }
          }
        }
      }

      // update the extent for the next mip
      extent.width = RDCMAX(extent.width >> 1, 1U);
      extent.height = RDCMAX(extent.height >> 1, 1U);
      extent.depth = RDCMAX(extent.depth >> 1, 1U);
    }
  }

  VkImageMemoryBarrier dstimBarrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_ALL_READ_BITS | VK_ACCESS_ALL_WRITE_BITS,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      curLayout,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(image),
      discardRange,
  };

  DoPipelineBarrier(cmd, 1, &dstimBarrier);

  if(!mainCopies.empty())
    ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), Unwrap(image),
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       (uint32_t)mainCopies.size(), mainCopies.data());

  if(!stencilCopies.empty())
    ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), Unwrap(image),
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       (uint32_t)stencilCopies.size(), stencilCopies.data());

  dstimBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  dstimBarrier.newLayout = curLayout;
  dstimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  dstimBarrier.dstAccessMask = VK_ACCESS_ALL_WRITE_BITS | VK_ACCESS_ALL_READ_BITS;

  DoPipelineBarrier(cmd, 1, &dstimBarrier);
}

void VulkanDebugManager::InitReadbackBuffer()
{
  if(m_ReadbackWindow.buf == VK_NULL_HANDLE)
  {
    VkDevice dev = m_pDriver->GetDev();
    m_ReadbackWindow.Create(m_pDriver, dev, 256 * 1024 * 1024ULL, 1, GPUBuffer::eGPUBufferReadback);

    VkResult vkr = ObjDisp(dev)->MapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem), 0,
                                           VK_WHOLE_SIZE, 0, (void **)&m_ReadbackPtr);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
}

void VulkanReplay::PatchReservedDescriptors(const VulkanStatePipeline &pipe,
                                            VkDescriptorPool &descpool,
                                            rdcarray<VkDescriptorSetLayout> &setLayouts,
                                            rdcarray<VkDescriptorSet> &descSets,
                                            VkShaderStageFlagBits patchedBindingStage,
                                            const VkDescriptorSetLayoutBinding *newBindings,
                                            size_t newBindingsCount)
{
  VkDevice dev = m_Device;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[pipe.pipeline];

  VkResult vkr = VK_SUCCESS;

  rdcarray<VkWriteDescriptorSet> descWrites;

  struct AllocedWrites
  {
    ~AllocedWrites()
    {
      for(VkDescriptorImageInfo *a : imgWrites)
        delete[] a;
      for(VkDescriptorBufferInfo *a : bufWrites)
        delete[] a;
      for(VkBufferView *a : bufViewWrites)
        delete[] a;
      for(VkWriteDescriptorSetInlineUniformBlockEXT *a : inlineWrites)
        delete a;
    }

    rdcarray<VkDescriptorImageInfo *> imgWrites;
    rdcarray<VkDescriptorBufferInfo *> bufWrites;
    rdcarray<VkBufferView *> bufViewWrites;
    rdcarray<VkWriteDescriptorSetInlineUniformBlockEXT *> inlineWrites;
  } alloced;

  rdcarray<VkDescriptorImageInfo *> &allocImgWrites = alloced.imgWrites;
  rdcarray<VkDescriptorBufferInfo *> &allocBufWrites = alloced.bufWrites;
  rdcarray<VkBufferView *> &allocBufViewWrites = alloced.bufViewWrites;

  rdcarray<VkWriteDescriptorSetInlineUniformBlockEXT *> &allocInlineWrites = alloced.inlineWrites;

  // one for each descriptor type. 1 of each to start with, we then increment for each descriptor
  // we need to allocate
  VkDescriptorPoolSize poolSizes[12] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1},
      {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, 0},
  };

  VkDescriptorPoolInlineUniformBlockCreateInfoEXT inlineCreateInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
  };

  static const uint32_t InlinePoolIndex = 11;

  uint32_t poolSizeCount = InlinePoolIndex;

  // count up our own
  for(size_t i = 0; i < newBindingsCount; i++)
  {
    RDCASSERT(newBindings[i].descriptorType < ARRAY_COUNT(poolSizes), newBindings[i].descriptorType);
    poolSizes[newBindings[i].descriptorType].descriptorCount += newBindings[i].descriptorCount;
  }

  const rdcarray<ResourceId> &pipeDescSetLayouts =
      creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts;

  // need to add our added bindings to the first descriptor set
  rdcarray<VkDescriptorSetLayoutBinding> bindings(newBindings, newBindingsCount);

  // if there are fewer sets bound than were declared in the pipeline layout, only process the
  // bound sets (as otherwise we'd fail to copy from them). Assume the application knew what it
  // was doing and the other sets are statically unused.
  setLayouts.resize(RDCMIN(pipe.descSets.size(), pipeDescSetLayouts.size()));

  size_t boundDescs = setLayouts.size();

  // need at least one set, if the shader isn't using any we'll just make our own
  if(setLayouts.empty())
    setLayouts.resize(1);

  // start with the limits as they are, and subtract off them incrementally. When any limit would
  // drop below 0, we fail.
  uint32_t maxPerStageDescriptorSamplers[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
  };
  uint32_t maxPerStageDescriptorUniformBuffers[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
  };
  uint32_t maxPerStageDescriptorStorageBuffers[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
  };
  uint32_t maxPerStageDescriptorSampledImages[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
  };
  uint32_t maxPerStageDescriptorStorageImages[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
  };
  uint32_t maxPerStageDescriptorInputAttachments[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
  };
  uint32_t maxPerStageResources[6] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
  };
  uint32_t maxDescriptorSetSamplers = m_pDriver->GetDeviceProps().limits.maxDescriptorSetSamplers;
  uint32_t maxDescriptorSetUniformBuffers =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetUniformBuffers;
  uint32_t maxDescriptorSetUniformBuffersDynamic =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetUniformBuffersDynamic;
  uint32_t maxDescriptorSetStorageBuffers =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetStorageBuffers;
  uint32_t maxDescriptorSetStorageBuffersDynamic =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetStorageBuffersDynamic;
  uint32_t maxDescriptorSetSampledImages =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetSampledImages;
  uint32_t maxDescriptorSetStorageImages =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetStorageImages;
  uint32_t maxDescriptorSetInputAttachments =
      m_pDriver->GetDeviceProps().limits.maxDescriptorSetInputAttachments;

  uint32_t maxDescriptorSetInlineUniformBlocks = 0;
  uint32_t maxPerStageDescriptorInlineUniformBlocks[6] = {};

  if(m_pDriver->GetExtensions(NULL).ext_EXT_inline_uniform_block)
  {
    VkPhysicalDeviceInlineUniformBlockPropertiesEXT inlineProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT,
    };

    VkPhysicalDeviceProperties2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    availBase.pNext = &inlineProps;
    m_pDriver->vkGetPhysicalDeviceProperties2(m_pDriver->GetPhysDev(), &availBase);

    maxDescriptorSetInlineUniformBlocks = inlineProps.maxDescriptorSetInlineUniformBlocks;
    for(size_t i = 0; i < ARRAY_COUNT(maxPerStageDescriptorInlineUniformBlocks); i++)
      maxPerStageDescriptorInlineUniformBlocks[i] =
          inlineProps.maxPerStageDescriptorInlineUniformBlocks;
  }

  bool error = false;

#define UPDATE_AND_CHECK_LIMIT(maxLimit)                                                   \
  if(!error)                                                                               \
  {                                                                                        \
    if(descriptorCount > maxLimit)                                                         \
    {                                                                                      \
      error = true;                                                                        \
      RDCWARN("Limit %s is exceeded. Cannot patch in required descriptor(s).", #maxLimit); \
    }                                                                                      \
    else                                                                                   \
    {                                                                                      \
      maxLimit -= descriptorCount;                                                         \
    }                                                                                      \
  }

#define UPDATE_AND_CHECK_STAGE_LIMIT(maxLimit)                                                 \
  if(!error)                                                                                   \
  {                                                                                            \
    for(uint32_t sbit = 0; sbit < 6; sbit++)                                                   \
    {                                                                                          \
      if(newBind.stageFlags & (1U << sbit))                                                    \
      {                                                                                        \
        if(descriptorCount > maxLimit[sbit])                                                   \
        {                                                                                      \
          error = true;                                                                        \
          RDCWARN("Limit %s is exceeded. Cannot patch in required descriptor(s).", #maxLimit); \
        }                                                                                      \
        else                                                                                   \
        {                                                                                      \
          maxLimit[sbit] -= descriptorCount;                                                   \
        }                                                                                      \
      }                                                                                        \
    }                                                                                          \
  }

  for(size_t i = 0; !error && i < setLayouts.size(); i++)
  {
    bool hasImmutableSamplers = false;

    // except for the first layout we need to start from scratch
    if(i > 0)
      bindings.clear();

    // if the shader had no descriptor sets at all, i will be invalid, so just skip and add a set
    // with only our own bindings.
    if(i < pipeDescSetLayouts.size() && i < pipe.descSets.size() &&
       pipe.descSets[i].pipeLayout != ResourceId())
    {
      // use the descriptor set layout from when it was bound. If the pipeline layout declared a
      // descriptor set layout for this set, but it's statically unused, it may be complete
      // garbage and doesn't match what the shader uses. However the pipeline layout at descriptor
      // set bind time must have been compatible and valid so we can use it. If this set *is* used
      // then the pipeline layout at bind time must be compatible with the pipeline's pipeline
      // layout, so we're fine too.
      const DescSetLayout &origLayout =
          creationInfo.m_DescSetLayout[creationInfo.m_PipelineLayout[pipe.descSets[i].pipeLayout]
                                           .descSetLayouts[i]];

      WrappedVulkan::DescriptorSetInfo &setInfo =
          m_pDriver->m_DescriptorSetState[pipe.descSets[i].descSet];

      for(size_t b = 0; !error && b < origLayout.bindings.size(); b++)
      {
        const DescSetLayout::Binding &bind = origLayout.bindings[b];

        // skip empty bindings
        if(bind.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          continue;

        uint32_t descriptorCount = bind.descriptorCount;

        if(bind.variableSize)
          descriptorCount = setInfo.data.variableDescriptorCount;

        // make room in the pool
        if(bind.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          poolSizes[InlinePoolIndex].descriptorCount += descriptorCount;
          inlineCreateInfo.maxInlineUniformBlockBindings++;
        }
        else
        {
          poolSizes[bind.descriptorType].descriptorCount += descriptorCount;
        }

        VkDescriptorSetLayoutBinding newBind;
        // offset the binding. We offset all sets to make it easier for patching - don't need to
        // conditionally patch shader bindings depending on which set they're in.
        newBind.binding = uint32_t(b + newBindingsCount);
        newBind.descriptorCount = descriptorCount;
        newBind.descriptorType = bind.descriptorType;

        // we only need it available for compute, just make all bindings visible otherwise dynamic
        // buffer offsets could be indexed wrongly. Consider the case where we have binding 0 as a
        // fragment UBO, and binding 1 as a vertex UBO. Then there are two dynamic offsets, and
        // the second is the one we want to use with ours. If we only add the compute visibility
        // bit to the second UBO, then suddenly it's the *first* offset that we must provide.
        // Instead of trying to remap offsets to match, we simply make every binding compute
        // visible so the ordering is still the same. Since compute and graphics are disjoint this
        // is safe.
        if(patchedBindingStage != 0)
          newBind.stageFlags = patchedBindingStage;
        else
          newBind.stageFlags = bind.stageFlags;

        switch(bind.descriptorType)
        {
          case VK_DESCRIPTOR_TYPE_SAMPLER:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetSamplers);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorSamplers);
            break;
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetSampledImages);
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetSamplers);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorSamplers);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorSampledImages);
            break;
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetSampledImages);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorSampledImages);
            break;
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetStorageImages);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorStorageImages);
            break;
          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetSampledImages);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorSampledImages);
            break;
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetStorageImages);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorStorageImages);
            break;
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetUniformBuffers);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorUniformBuffers);
            break;
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetStorageBuffers);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorStorageBuffers);
            break;
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetUniformBuffersDynamic);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorUniformBuffers);
            break;
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetStorageBuffersDynamic);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorStorageBuffers);
            break;
          case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetInputAttachments);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorInputAttachments);
            break;
          case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
            descriptorCount = 1;
            UPDATE_AND_CHECK_LIMIT(maxDescriptorSetInlineUniformBlocks);
            UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorInlineUniformBlocks);
            break;
          default: break;
        }

        UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageResources);

        if(bind.immutableSampler)
        {
          hasImmutableSamplers = true;
          VkSampler *samplers = new VkSampler[bind.descriptorCount];
          newBind.pImmutableSamplers = samplers;
          for(uint32_t s = 0; s < bind.descriptorCount; s++)
            samplers[s] = GetResourceManager()->GetCurrentHandle<VkSampler>(bind.immutableSampler[s]);
        }
        else
        {
          newBind.pImmutableSamplers = NULL;
        }

        bindings.push_back(newBind);
      }
    }

    VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        (uint32_t)bindings.size(),
        bindings.data(),
    };

    if(!error)
    {
      // create new offseted descriptor layout
      vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL, &setLayouts[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    if(hasImmutableSamplers)
    {
      for(const VkDescriptorSetLayoutBinding &bind : bindings)
        delete[] bind.pImmutableSamplers;
    }
  }

  // if we hit an error, we can't create the descriptor set so bail out now
  if(error)
    return;

  VkDescriptorPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  // 1 set for each layout
  poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolCreateInfo.maxSets = (uint32_t)setLayouts.size();
  poolCreateInfo.poolSizeCount = poolSizeCount;
  poolCreateInfo.pPoolSizes = poolSizes;

  if(inlineCreateInfo.maxInlineUniformBlockBindings > 0)
  {
    poolCreateInfo.poolSizeCount++;
    poolCreateInfo.pNext = &inlineCreateInfo;
  }

  // create descriptor pool with enough space for our descriptors
  vkr = m_pDriver->vkCreateDescriptorPool(dev, &poolCreateInfo, NULL, &descpool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // allocate all the descriptors
  VkDescriptorSetAllocateInfo descSetAllocInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      NULL,
      descpool,
      (uint32_t)setLayouts.size(),
      setLayouts.data(),
  };

  descSets.resize(setLayouts.size());
  m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, descSets.data());

  // copy the data across from the real descriptors into our adjusted bindings
  for(size_t i = 0; i < boundDescs; i++)
  {
    if(pipe.descSets[i].descSet == ResourceId())
      continue;

    // as above we use the pipeline layout that was originally used to bind this descriptor set
    // and not the pipeline layout from the pipeline, in case the pipeline statically doesn't use
    // this set and so its descriptor set layout is garbage (doesn't match the actual bound
    // descriptor set)
    const DescSetLayout &origLayout =
        creationInfo.m_DescSetLayout[creationInfo.m_PipelineLayout[pipe.descSets[i].pipeLayout]
                                         .descSetLayouts[i]];

    WrappedVulkan::DescriptorSetInfo &setInfo =
        m_pDriver->m_DescriptorSetState[pipe.descSets[i].descSet];

    {
      // push descriptors don't have a source to copy from, we need to add writes
      VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      write.dstSet = descSets[i];

      // Only write bindings that actually exist in the current descriptor
      // set. If there are bindings that aren't set, assume the app knows
      // what it's doing and the remaining bindings are unused.
      for(size_t b = 0; b < setInfo.data.binds.size(); b++)
      {
        const DescSetLayout::Binding &bind = origLayout.bindings[b];

        // skip empty bindings
        if(bind.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          continue;

        uint32_t descriptorCount = bind.descriptorCount;

        if(bind.variableSize)
          descriptorCount = setInfo.data.variableDescriptorCount;

        DescriptorSetSlot *slot = setInfo.data.binds[b];

        write.dstBinding = uint32_t(b + newBindingsCount);
        write.dstArrayElement = 0;
        write.descriptorCount = descriptorCount;
        write.descriptorType = bind.descriptorType;

        switch(write.descriptorType)
        {
          case VK_DESCRIPTOR_TYPE_SAMPLER:
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
          {
            VkDescriptorImageInfo *out = new VkDescriptorImageInfo[write.descriptorCount];
            for(uint32_t w = 0; w < write.descriptorCount; w++)
            {
              const DescriptorSetSlotImageInfo &src = slot[w].imageInfo;

              out[w].imageLayout = src.imageLayout;
              out[w].sampler = GetResourceManager()->GetCurrentHandle<VkSampler>(src.sampler);
              out[w].imageView = GetResourceManager()->GetCurrentHandle<VkImageView>(src.imageView);
            }

            write.pImageInfo = out;
            allocImgWrites.push_back(out);
            break;
          }
          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          {
            VkBufferView *out = new VkBufferView[write.descriptorCount];
            for(uint32_t w = 0; w < write.descriptorCount; w++)
              out[w] = GetResourceManager()->GetCurrentHandle<VkBufferView>(slot[w].texelBufferView);
            write.pTexelBufferView = out;
            allocBufViewWrites.push_back(out);
            break;
          }
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
          {
            VkDescriptorBufferInfo *out = new VkDescriptorBufferInfo[write.descriptorCount];
            for(uint32_t w = 0; w < write.descriptorCount; w++)
            {
              const DescriptorSetSlotBufferInfo &src = slot[w].bufferInfo;

              out[w].offset = src.offset;
              out[w].range = src.range;
              out[w].buffer = GetResourceManager()->GetCurrentHandle<VkBuffer>(src.buffer);
            }
            write.pBufferInfo = out;
            allocBufWrites.push_back(out);
            break;
          }
          case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
          {
            allocInlineWrites.push_back(new VkWriteDescriptorSetInlineUniformBlockEXT);
            VkWriteDescriptorSetInlineUniformBlockEXT *inlineWrite = allocInlineWrites.back();
            inlineWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
            inlineWrite->pNext = NULL;
            inlineWrite->dataSize = descriptorCount;
            inlineWrite->pData = setInfo.data.inlineBytes.data() + slot[0].inlineOffset;
            write.pNext = inlineWrite;
            break;
          }
          default: RDCERR("Unexpected descriptor type %d", write.descriptorType);
        }

        // skip validity check for inline uniform block as the descriptor count means something
        // different
        if(write.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          write.descriptorCount = descriptorCount;
          descWrites.push_back(write);
          continue;
        }

        // start with no descriptors
        write.descriptorCount = 0;

        for(uint32_t w = 0; w < descriptorCount; w++)
        {
          // if this write is valid, we increment the descriptor count and continue
          if(IsValid(m_pDriver->NULLDescriptorsAllowed(), write, w - write.dstArrayElement))
          {
            write.descriptorCount++;
          }
          else
          {
            // if this write isn't valid, then we first check to see if we had any previous
            // pending writes in the array we were going to batch together, if so we add them.
            if(write.descriptorCount > 0)
              descWrites.push_back(write);

            // skip past any previous descriptors we just wrote, as well as the current invalid
            // one
            if(write.pBufferInfo)
              write.pBufferInfo += write.descriptorCount + 1;
            if(write.pImageInfo)
              write.pImageInfo += write.descriptorCount + 1;
            if(write.pTexelBufferView)
              write.pTexelBufferView += write.descriptorCount + 1;

            // now start again from 0 descriptors, at the next array element
            write.dstArrayElement += write.descriptorCount + 1;
            write.descriptorCount = 0;
          }
        }

        // if there are any left, add them here
        if(write.descriptorCount > 0)
          descWrites.push_back(write);

        // don't leak the arrays and cause double deletes, NULL them after each time
        write.pImageInfo = NULL;
        write.pBufferInfo = NULL;
        write.pTexelBufferView = NULL;
      }
    }
  }

  m_pDriver->vkUpdateDescriptorSets(dev, (uint32_t)descWrites.size(), descWrites.data(), 0, NULL);
}

void VulkanDebugManager::CustomShaderRendering::Destroy(WrappedVulkan *driver)
{
  driver->vkDestroyRenderPass(driver->GetDev(), TexRP, NULL);
  driver->vkDestroyFramebuffer(driver->GetDev(), TexFB, NULL);
  driver->vkDestroyImage(driver->GetDev(), TexImg, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(TexImgView); i++)
    driver->vkDestroyImageView(driver->GetDev(), TexImgView[i], NULL);
  driver->vkFreeMemory(driver->GetDev(), TexMem, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), TexPipeline, NULL);
}

void VulkanReplay::CreateResources()
{
  m_Device = m_pDriver->GetDev();

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.0f);

  m_General.Init(m_pDriver, VK_NULL_HANDLE);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.1f);

  m_TexRender.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.3f);

  m_Overlay.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.4f);

  m_MeshRender.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.6f);

  m_VertexPick.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.7f);

  m_PixelPick.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.75f);

  m_PixelHistory.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.8f);

  m_Histogram.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.9f);

  m_ShaderDebugData.Init(m_pDriver, m_General.DescriptorPool);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 1.0f);

  GPA_vkContextOpenInfo context = {Unwrap(m_pDriver->GetInstance()),
                                   Unwrap(m_pDriver->GetPhysDev()), Unwrap(m_pDriver->GetDev())};

  if(!m_pDriver->GetReplay()->IsRemoteProxy() && Vulkan_HardwareCounters())
  {
    AMDCounters *counters = NULL;

    GPUVendor vendor = m_pDriver->GetDriverInfo().Vendor();

    if(vendor == GPUVendor::AMD)
    {
      RDCLOG("AMD GPU detected - trying to initialise AMD counters");
      counters = new AMDCounters();
    }
    else
    {
      RDCLOG("%s GPU detected - no counters available", ToStr(vendor).c_str());
    }

    if(counters && counters->Init(AMDCounters::ApiType::Vk, (void *)&context))
    {
      m_pAMDCounters = counters;
    }
    else
    {
      delete counters;
      m_pAMDCounters = NULL;
    }
  }
}

void VulkanReplay::DestroyResources()
{
  ClearPostVSCache();
  ClearFeedbackCache();

  m_General.Destroy(m_pDriver);
  m_TexRender.Destroy(m_pDriver);
  m_Overlay.Destroy(m_pDriver);
  m_VertexPick.Destroy(m_pDriver);
  m_PixelPick.Destroy(m_pDriver);
  m_PixelHistory.Destroy(m_pDriver);
  m_Histogram.Destroy(m_pDriver);
  m_PostVS.Destroy(m_pDriver);

  SAFE_DELETE(m_pAMDCounters);
}

void VulkanReplay::GeneralMisc::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  VkResult vkr = VK_SUCCESS;

  VkDescriptorPoolSize descPoolTypes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 320},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 128},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 64},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
  };

  VkDescriptorPoolCreateInfo descPoolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      32,
      ARRAY_COUNT(descPoolTypes),
      &descPoolTypes[0],
  };

  // create descriptor pool
  vkr = driver->vkCreateDescriptorPool(driver->GetDev(), &descPoolInfo, NULL, &DescriptorPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  CREATE_OBJECT(PointSampler, VK_FILTER_NEAREST);
}

void VulkanReplay::GeneralMisc::Destroy(WrappedVulkan *driver)
{
  if(DescriptorPool == VK_NULL_HANDLE)
    return;

  driver->vkDestroyDescriptorPool(driver->GetDev(), DescriptorPool, NULL);
  driver->vkDestroySampler(driver->GetDev(), PointSampler, NULL);
}

void VulkanReplay::TextureRendering::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  VkResult vkr = VK_SUCCESS;

  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  CREATE_OBJECT(PointSampler, VK_FILTER_NEAREST);
  CREATE_OBJECT(LinearSampler, VK_FILTER_LINEAR);

  CREATE_OBJECT(DescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
                    {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, VK_SHADER_STAGE_ALL, NULL},
                    {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {15, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {50, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, &PointSampler},
                    {51, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, &LinearSampler},
                });

  CREATE_OBJECT(PipeLayout, DescSetLayout, 0);

  for(size_t i = 0; i < ARRAY_COUNT(DescSet); i++)
  {
    CREATE_OBJECT(DescSet[i], descriptorPool, DescSetLayout);
  }

  UBO.Create(driver, driver->GetDev(), 128, 10, 0);
  RDCCOMPILE_ASSERT(sizeof(TexDisplayUBOData) <= 128, "tex display size");

  HeatmapUBO.Create(driver, driver->GetDev(), 512, 10, 0);
  RDCCOMPILE_ASSERT(sizeof(HeatmapData) <= 512, "tex display size");

  {
    VkRenderPass SRGBA8RP = VK_NULL_HANDLE;
    VkRenderPass RGBA16RP = VK_NULL_HANDLE;
    VkRenderPass RGBA32RP = VK_NULL_HANDLE;

    CREATE_OBJECT(SRGBA8RP, VK_FORMAT_R8G8B8A8_SRGB);
    CREATE_OBJECT(RGBA16RP, VK_FORMAT_R16G16B16A16_SFLOAT);
    CREATE_OBJECT(RGBA32RP, VK_FORMAT_R32G32B32A32_SFLOAT);

    ConciseGraphicsPipeline texDisplayInfo = {
        SRGBA8RP,
        PipeLayout,
        shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
        shaderCache->GetBuiltinModule(BuiltinShader::TexDisplayFS),
        {VK_DYNAMIC_STATE_VIEWPORT},
        VK_SAMPLE_COUNT_1_BIT,
        false,    // sampleRateShading
        false,    // depthEnable
        false,    // stencilEnable
        VK_STENCIL_OP_KEEP,
        true,     // colourOutput
        false,    // blendEnable
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ZERO,
        0xf,    // writeMask
    };

    ConciseGraphicsPipeline texRemapInfo = texDisplayInfo;

    CREATE_OBJECT(Pipeline, texDisplayInfo);

    texDisplayInfo.renderPass = RGBA32RP;
    CREATE_OBJECT(F32Pipeline, texDisplayInfo);

    texDisplayInfo.renderPass = RGBA16RP;
    CREATE_OBJECT(F16Pipeline, texDisplayInfo);

    texDisplayInfo.renderPass = SRGBA8RP;
    texDisplayInfo.blendEnable = true;
    texDisplayInfo.srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
    texDisplayInfo.dstBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    CREATE_OBJECT(BlendPipeline, texDisplayInfo);

    VkFormat formats[3] = {VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R16G16B16A16_UINT,
                           VK_FORMAT_R32G32B32A32_UINT};
    CompType cast[3] = {CompType::Float, CompType::UInt, CompType::SInt};

    for(int f = 0; f < 3; f++)
    {
      for(int i = 0; i < 3; i++)
      {
        texRemapInfo.fragment =
            shaderCache->GetBuiltinModule(BuiltinShader::TexRemap, BuiltinShaderBaseType(i));

        CREATE_OBJECT(texRemapInfo.renderPass, GetViewCastedFormat(formats[f], cast[i]));

        CREATE_OBJECT(RemapPipeline[f][i][0], texRemapInfo);

        driver->vkDestroyRenderPass(driver->GetDev(), texRemapInfo.renderPass, NULL);
      }
    }

    // make versions that only write to green, for doing two-pass stencil writes
    texRemapInfo.writeMask = texDisplayInfo.writeMask = 0x2;

    for(int f = 0; f < 3; f++)
    {
      for(int i = 0; i < 3; i++)
      {
        texRemapInfo.fragment =
            shaderCache->GetBuiltinModule(BuiltinShader::TexRemap, BuiltinShaderBaseType(i));

        CREATE_OBJECT(texRemapInfo.renderPass, GetViewCastedFormat(formats[f], cast[i]));

        CREATE_OBJECT(RemapPipeline[f][i][1], texRemapInfo);

        driver->vkDestroyRenderPass(driver->GetDev(), texRemapInfo.renderPass, NULL);
      }
    }

    texDisplayInfo.renderPass = SRGBA8RP;
    CREATE_OBJECT(PipelineGreenOnly, texDisplayInfo);

    texDisplayInfo.renderPass = RGBA32RP;
    CREATE_OBJECT(F32PipelineGreenOnly, texDisplayInfo);

    texDisplayInfo.renderPass = RGBA16RP;
    CREATE_OBJECT(F16PipelineGreenOnly, texDisplayInfo);

    driver->vkDestroyRenderPass(driver->GetDev(), SRGBA8RP, NULL);
    driver->vkDestroyRenderPass(driver->GetDev(), RGBA16RP, NULL);
    driver->vkDestroyRenderPass(driver->GetDev(), RGBA32RP, NULL);
  }

  // create dummy images for filling out the texdisplay descriptors
  // in slots that are skipped by dynamic branching (e.g. 3D texture
  // when we're displaying a 2D, etc).
  {
    VkCommandBuffer cmd = driver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    int index = 0;

    // we pick RGBA8 formats to be guaranteed they will be supported
    VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_SINT};
    VkImageType types[] = {VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_2D};
    VkImageViewType viewtypes[] = {
        VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_3D,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        driver->GetDeviceEnabledFeatures().imageCubeArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                                          : VK_IMAGE_VIEW_TYPE_CUBE,
    };
    VkSampleCountFlagBits sampleCounts[] = {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_1_BIT,
                                            VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT};

    // type max is one higher than the last RESTYPE, and RESTYPES are 1-indexed
    RDCCOMPILE_ASSERT(RESTYPE_TEXTYPEMAX - 1 == ARRAY_COUNT(types),
                      "RESTYPE values don't match formats for dummy images");

    RDCCOMPILE_ASSERT(sizeof(DummyImages) == sizeof(DummyImageViews),
                      "dummy image arrays mismatched sizes");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(DummyImages) == ARRAY_COUNT(formats),
                      "dummy image arrays mismatched sizes");
    // types + 1 for cube
    RDCCOMPILE_ASSERT(ARRAY_COUNT(DummyImages[0]) == ARRAY_COUNT(types) + 1,
                      "dummy image arrays mismatched sizes");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(DummyImages[0]) == ARRAY_COUNT(viewtypes),
                      "dummy image arrays mismatched sizes");
    RDCCOMPILE_ASSERT(ARRAY_COUNT(DummyWrites) == ARRAY_COUNT(DummyInfos),
                      "dummy image arrays mismatched sizes");

    CREATE_OBJECT(DummySampler, VK_FILTER_NEAREST);

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
            VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            NULL,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        // make the 2D image cube-compatible
        if(type == 1)
        {
          imInfo.arrayLayers = 6;
          imInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &DummyImages[fmt][type]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        MemoryAllocation alloc = driver->AllocateMemoryForResource(
            DummyImages[fmt][type], MemoryScope::ImmutableReplayDebug, MemoryType::GPULocal);

        vkr = driver->vkBindImageMemory(driver->GetDev(), DummyImages[fmt][type], alloc.mem,
                                        alloc.offs);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        // fill out the descriptor set write to the write binding - set will be filled out
        // on demand when we're actually using these writes.
        DummyWrites[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DummyWrites[index].pNext = NULL;
        DummyWrites[index].dstSet = VK_NULL_HANDLE;
        DummyWrites[index].dstBinding =
            5 * uint32_t(fmt + 1) + uint32_t(type) + 1;    // 5 + RESTYPE_x
        DummyWrites[index].dstArrayElement = 0;
        DummyWrites[index].descriptorCount = 1;
        DummyWrites[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        DummyWrites[index].pImageInfo = &DummyInfos[index];
        DummyWrites[index].pBufferInfo = NULL;
        DummyWrites[index].pTexelBufferView = NULL;

        DummyInfos[index].sampler = Unwrap(DummySampler);
        DummyInfos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        index++;
      }
    }

    // add the last one for the odd-one-out YUV texture

    DummyWrites[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DummyWrites[index].pNext = NULL;
    DummyWrites[index].dstSet = VK_NULL_HANDLE;
    DummyWrites[index].dstBinding = 10;    // texYUV
    DummyWrites[index].dstArrayElement = 0;
    DummyWrites[index].descriptorCount = 1;
    DummyWrites[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    DummyWrites[index].pImageInfo = &DummyInfos[index];
    DummyWrites[index].pBufferInfo = NULL;
    DummyWrites[index].pTexelBufferView = NULL;

    DummyWrites[index + 1] = DummyWrites[index];
    DummyWrites[index + 1].dstArrayElement = 1;

    DummyInfos[index].sampler = Unwrap(DummySampler);
    DummyInfos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    DummyInfos[index + 1].sampler = Unwrap(DummySampler);
    DummyInfos[index + 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // align up for the dummy buffer
    {
      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,     NULL, 0, 16,
          VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
      };

      vkr = driver->vkCreateBuffer(driver->GetDev(), &bufInfo, NULL, &DummyBuffer);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      MemoryAllocation alloc = driver->AllocateMemoryForResource(
          DummyBuffer, MemoryScope::ImmutableReplayDebug, MemoryType::GPULocal);

      vkr = driver->vkBindBufferMemory(driver->GetDev(), DummyBuffer, alloc.mem, alloc.offs);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    // now that the image memory is bound, we can create the image views and fill the descriptor
    // set writes.
    index = 0;
    for(size_t fmt = 0; fmt < ARRAY_COUNT(formats); fmt++)
    {
      for(size_t type = 0; type < ARRAY_COUNT(viewtypes); type++)
      {
        size_t imType = type;

        // the cubemap view re-uses the 2D image
        bool cube = false;
        if(viewtypes[type] == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY ||
           viewtypes[type] == VK_IMAGE_VIEW_TYPE_CUBE)
        {
          imType = 1;
          cube = true;
        }

        VkImageViewCreateInfo viewInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            NULL,
            0,
            DummyImages[fmt][imType],
            viewtypes[type],
            formats[fmt],
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
            },
        };

        if(cube)
          viewInfo.subresourceRange.layerCount = 6;

        vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL,
                                        &DummyImageViews[fmt][type]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        // the cubemap view we don't create an info for it, and the image is already transitioned
        if(cube)
          continue;

        RDCASSERT((size_t)index < ARRAY_COUNT(DummyInfos), index);

        DummyInfos[index].imageView = Unwrap(DummyImageViews[fmt][type]);

        // need to update image layout into valid state
        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            Unwrap(DummyImages[fmt][imType]),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        DoPipelineBarrier(cmd, 1, &barrier);

        index++;
      }
    }

    // duplicate 2D dummy image into YUV
    DummyInfos[index].imageView = DummyInfos[1].imageView;
    DummyInfos[index + 1].imageView = DummyInfos[1].imageView;

    if(DummyBuffer != VK_NULL_HANDLE)
    {
      VkFormat bufViewTypes[] = {
          VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SINT,
      };
      for(size_t i = 0; i < ARRAY_COUNT(bufViewTypes); i++)
      {
        VkBufferViewCreateInfo viewInfo = {
            VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL, 0, DummyBuffer, bufViewTypes[i], 0, 16,
        };

        vkr = driver->vkCreateBufferView(driver->GetDev(), &viewInfo, NULL, &DummyBufferView[i]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }
    }

    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  }
}

void VulkanReplay::TextureRendering::Destroy(WrappedVulkan *driver)
{
  if(DescSetLayout == VK_NULL_HANDLE)
    return;

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), DescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), PipeLayout, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), Pipeline, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), BlendPipeline, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), F16Pipeline, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), F32Pipeline, NULL);
  for(size_t f = 0; f < 3; f++)
    for(size_t i = 0; i < 3; i++)
      for(size_t g = 0; g < 2; g++)
        driver->vkDestroyPipeline(driver->GetDev(), RemapPipeline[f][i][g], NULL);

  driver->vkDestroyPipeline(driver->GetDev(), PipelineGreenOnly, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), F16PipelineGreenOnly, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), F32PipelineGreenOnly, NULL);
  UBO.Destroy();
  HeatmapUBO.Destroy();

  driver->vkDestroySampler(driver->GetDev(), PointSampler, NULL);
  driver->vkDestroySampler(driver->GetDev(), LinearSampler, NULL);

  for(size_t fmt = 0; fmt < ARRAY_COUNT(DummyImages); fmt++)
  {
    for(size_t type = 0; type < ARRAY_COUNT(DummyImages[0]); type++)
    {
      driver->vkDestroyImageView(driver->GetDev(), DummyImageViews[fmt][type], NULL);
      driver->vkDestroyImage(driver->GetDev(), DummyImages[fmt][type], NULL);
    }
  }

  for(size_t fmt = 0; fmt < ARRAY_COUNT(DummyBufferView); fmt++)
    driver->vkDestroyBufferView(driver->GetDev(), DummyBufferView[fmt], NULL);
  driver->vkDestroyBuffer(driver->GetDev(), DummyBuffer, NULL);

  driver->vkDestroySampler(driver->GetDev(), DummySampler, NULL);
}

void VulkanReplay::OverlayRendering::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  VkRenderPass SRGBA8RP = VK_NULL_HANDLE;
  VkRenderPass SRGBA8MSRP = VK_NULL_HANDLE;

  CREATE_OBJECT(SRGBA8RP, VK_FORMAT_R8G8B8A8_SRGB);
  CREATE_OBJECT(SRGBA8MSRP, VK_FORMAT_R8G8B8A8_SRGB, VULKAN_MESH_VIEW_SAMPLES);

  CREATE_OBJECT(m_CheckerDescSetLayout,
                {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL}});

  CREATE_OBJECT(m_QuadDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  CREATE_OBJECT(m_TriSizeDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
                    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  CREATE_OBJECT(m_CheckerPipeLayout, m_CheckerDescSetLayout, 0);
  CREATE_OBJECT(m_QuadResolvePipeLayout, m_QuadDescSetLayout, 0);
  CREATE_OBJECT(m_TriSizePipeLayout, m_TriSizeDescSetLayout, 0);
  CREATE_OBJECT(m_QuadDescSet, descriptorPool, m_QuadDescSetLayout);
  CREATE_OBJECT(m_TriSizeDescSet, descriptorPool, m_TriSizeDescSetLayout);
  CREATE_OBJECT(m_CheckerDescSet, descriptorPool, m_CheckerDescSetLayout);

  m_CheckerUBO.Create(driver, driver->GetDev(), 128, 10, 0);
  RDCCOMPILE_ASSERT(sizeof(CheckerboardUBOData) <= 128, "checkerboard UBO size");

  m_TriSizeUBO.Create(driver, driver->GetDev(), sizeof(Vec4f), 4096, 0);

  ConciseGraphicsPipeline pipeInfo = {
      SRGBA8RP,
      m_CheckerPipeLayout,
      shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
      shaderCache->GetBuiltinModule(BuiltinShader::CheckerboardFS),
      {VK_DYNAMIC_STATE_VIEWPORT},
      VK_SAMPLE_COUNT_1_BIT,
      false,    // sampleRateShading
      false,    // depthEnable
      false,    // stencilEnable
      VK_STENCIL_OP_KEEP,
      true,     // colourOutput
      false,    // blendEnable
      VK_BLEND_FACTOR_SRC_ALPHA,
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      0xf,    // writeMask
  };

  CREATE_OBJECT(m_CheckerPipeline, pipeInfo);

  pipeInfo.renderPass = SRGBA8MSRP;
  pipeInfo.sampleCount = VULKAN_MESH_VIEW_SAMPLES;

  CREATE_OBJECT(m_CheckerMSAAPipeline, pipeInfo);

  uint32_t samplesHandled = 0;

  RDCCOMPILE_ASSERT(ARRAY_COUNT(m_CheckerF16Pipeline) == ARRAY_COUNT(m_QuadResolvePipeline),
                    "Arrays are mismatched in size!");

  uint32_t supportedSampleCounts = driver->GetDeviceProps().limits.framebufferColorSampleCounts;

  for(size_t i = 0; i < ARRAY_COUNT(m_CheckerF16Pipeline); i++)
  {
    VkSampleCountFlagBits samples = VkSampleCountFlagBits(1 << i);

    if((supportedSampleCounts & (uint32_t)samples) == 0)
      continue;

    VkRenderPass RGBA16MSRP = VK_NULL_HANDLE;

    CREATE_OBJECT(RGBA16MSRP, VK_FORMAT_R16G16B16A16_SFLOAT, samples);

    if(RGBA16MSRP != VK_NULL_HANDLE)
      samplesHandled |= (uint32_t)samples;
    else
      continue;

    // if we this sample count is supported then create a pipeline
    pipeInfo.renderPass = RGBA16MSRP;
    pipeInfo.sampleCount = VkSampleCountFlagBits(1 << i);

    // set up outline pipeline configuration
    pipeInfo.blendEnable = true;
    pipeInfo.fragment = shaderCache->GetBuiltinModule(BuiltinShader::CheckerboardFS);
    pipeInfo.pipeLayout = m_CheckerPipeLayout;

    CREATE_OBJECT(m_CheckerF16Pipeline[i], pipeInfo);

    // set up quad resolve pipeline configuration
    pipeInfo.blendEnable = false;
    pipeInfo.fragment = shaderCache->GetBuiltinModule(BuiltinShader::QuadResolveFS);
    pipeInfo.pipeLayout = m_QuadResolvePipeLayout;

    if(pipeInfo.fragment != VK_NULL_HANDLE &&
       shaderCache->GetBuiltinModule(BuiltinShader::QuadWriteFS) != VK_NULL_HANDLE)
    {
      CREATE_OBJECT(m_QuadResolvePipeline[i], pipeInfo);
    }

    driver->vkDestroyRenderPass(driver->GetDev(), RGBA16MSRP, NULL);
  }

  RDCASSERTEQUAL((uint32_t)driver->GetDeviceProps().limits.framebufferColorSampleCounts,
                 samplesHandled);

  VkDescriptorBufferInfo checkerboard = {};
  m_CheckerUBO.FillDescriptor(checkerboard);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_CheckerDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &checkerboard, NULL},
  };

  VkDevice dev = driver->GetDev();

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writes), writes, 0, NULL);

  driver->vkDestroyRenderPass(driver->GetDev(), SRGBA8RP, NULL);
  driver->vkDestroyRenderPass(driver->GetDev(), SRGBA8MSRP, NULL);
}

void VulkanReplay::OverlayRendering::Destroy(WrappedVulkan *driver)
{
  if(ImageMem == VK_NULL_HANDLE)
    return;

  driver->vkFreeMemory(driver->GetDev(), ImageMem, NULL);
  driver->vkDestroyImage(driver->GetDev(), Image, NULL);
  driver->vkDestroyImageView(driver->GetDev(), ImageView, NULL);
  driver->vkDestroyFramebuffer(driver->GetDev(), NoDepthFB, NULL);
  driver->vkDestroyRenderPass(driver->GetDev(), NoDepthRP, NULL);

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), m_QuadDescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), m_QuadResolvePipeLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_QuadResolvePipeline); i++)
    driver->vkDestroyPipeline(driver->GetDev(), m_QuadResolvePipeline[i], NULL);

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), m_CheckerDescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), m_CheckerPipeLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_CheckerF16Pipeline); i++)
    driver->vkDestroyPipeline(driver->GetDev(), m_CheckerF16Pipeline[i], NULL);
  driver->vkDestroyPipeline(driver->GetDev(), m_CheckerPipeline, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), m_CheckerMSAAPipeline, NULL);

  m_CheckerUBO.Destroy();

  m_TriSizeUBO.Destroy();
  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), m_TriSizeDescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), m_TriSizePipeLayout, NULL);
}

void VulkanReplay::MeshRendering::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  CREATE_OBJECT(DescSetLayout,
                {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL}});

  CREATE_OBJECT(PipeLayout, DescSetLayout, 0);
  CREATE_OBJECT(DescSet, descriptorPool, DescSetLayout);

  UBO.Create(driver, driver->GetDev(), sizeof(MeshUBOData), 16, 0);
  BBoxVB.Create(driver, driver->GetDev(), sizeof(Vec4f) * 128, 16, GPUBuffer::eGPUBufferVBuffer);

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
  AxisFrustumVB.Create(driver, driver->GetDev(), sizeof(axisFrustum), 1,
                       GPUBuffer::eGPUBufferVBuffer);

  Vec4f *axisData = (Vec4f *)AxisFrustumVB.Map();

  memcpy(axisData, axisFrustum, sizeof(axisFrustum));

  AxisFrustumVB.Unmap();

  VkDescriptorBufferInfo meshrender = {};

  UBO.FillDescriptor(meshrender);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(DescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &meshrender, NULL},
  };

  VkDevice dev = driver->GetDev();

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writes), writes, 0, NULL);
}

void VulkanReplay::MeshRendering::Destroy(WrappedVulkan *driver)
{
  if(DescSetLayout == VK_NULL_HANDLE)
    return;

  UBO.Destroy();
  BBoxVB.Destroy();
  AxisFrustumVB.Destroy();

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), DescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), PipeLayout, NULL);
}

void VulkanReplay::VertexPicking::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  CREATE_OBJECT(DescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  CREATE_OBJECT(Layout, DescSetLayout, 0);
  CREATE_OBJECT(DescSet, descriptorPool, DescSetLayout);

  // sizes are always 0 so that these buffers are created on demand
  IBSize = 0;
  VBSize = 0;

  UBO.Create(driver, driver->GetDev(), 128, 1, 0);
  RDCCOMPILE_ASSERT(sizeof(MeshPickUBOData) <= 128, "mesh pick UBO size");

  const size_t meshPickResultSize = MaxMeshPicks * sizeof(FloatVector) + sizeof(uint32_t);

  Result.Create(driver, driver->GetDev(), meshPickResultSize, 1,
                GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
  ResultReadback.Create(driver, driver->GetDev(), meshPickResultSize, 1,
                        GPUBuffer::eGPUBufferReadback);

  CREATE_OBJECT(Pipeline, Layout, shaderCache->GetBuiltinModule(BuiltinShader::MeshCS));

  VkDescriptorBufferInfo vertexpickUBO = {};
  VkDescriptorBufferInfo vertexpickResult = {};

  UBO.FillDescriptor(vertexpickUBO);
  Result.FillDescriptor(vertexpickResult);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(DescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &vertexpickUBO, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(DescSet), 3, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &vertexpickResult, NULL},
  };

  VkDevice dev = driver->GetDev();

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writes), writes, 0, NULL);
}

void VulkanReplay::VertexPicking::Destroy(WrappedVulkan *driver)
{
  if(DescSetLayout == VK_NULL_HANDLE)
    return;

  UBO.Destroy();
  IB.Destroy();
  IBUpload.Destroy();
  VB.Destroy();
  VBUpload.Destroy();
  Result.Destroy();
  ResultReadback.Destroy();

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), DescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), Layout, NULL);
  driver->vkDestroyPipeline(driver->GetDev(), Pipeline, NULL);
}

void VulkanReplay::PixelPicking::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  VkResult vkr = VK_SUCCESS;

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

  vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &Image);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};
  driver->vkGetImageMemoryRequirements(driver->GetDev(), Image, &mrq);

  // allocate readback memory
  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
      driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
  };

  vkr = driver->vkAllocateMemory(driver->GetDev(), &allocInfo, NULL, &ImageMem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = driver->vkBindImageMemory(driver->GetDev(), Image, ImageMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      Image,
      VK_IMAGE_VIEW_TYPE_2D,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
      },
  };

  vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &ImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // need to update image layout into valid state

  VkCommandBuffer cmd = driver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(Image),
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };

  DoPipelineBarrier(cmd, 1, &barrier);

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  CREATE_OBJECT(RP, VK_FORMAT_R32G32B32A32_SFLOAT);

  // create framebuffer
  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, RP, 1, &ImageView, 1, 1, 1,
  };

  vkr = driver->vkCreateFramebuffer(driver->GetDev(), &fbinfo, NULL, &FB);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // since we always sync for readback, doesn't need to be ring'd
  ReadbackBuffer.Create(driver, driver->GetDev(), sizeof(float) * 4, 1,
                        GPUBuffer::eGPUBufferReadback);
}

void VulkanReplay::PixelPicking::Destroy(WrappedVulkan *driver)
{
  if(Image == VK_NULL_HANDLE)
    return;

  driver->vkDestroyImage(driver->GetDev(), Image, NULL);
  driver->vkFreeMemory(driver->GetDev(), ImageMem, NULL);
  driver->vkDestroyImageView(driver->GetDev(), ImageView, NULL);
  ReadbackBuffer.Destroy();
  driver->vkDestroyFramebuffer(driver->GetDev(), FB, NULL);
  driver->vkDestroyRenderPass(driver->GetDev(), RP, NULL);
}

void VulkanReplay::PixelHistory::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  CREATE_OBJECT(MSCopyDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  VkResult vkr = VK_SUCCESS;
  VkDescriptorPoolSize descPoolTypes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
  };

  VkDescriptorPoolCreateInfo descPoolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      32,
      ARRAY_COUNT(descPoolTypes),
      &descPoolTypes[0],
  };

  // create descriptor pool
  vkr = driver->vkCreateDescriptorPool(driver->GetDev(), &descPoolInfo, NULL, &MSCopyDescPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  CREATE_OBJECT(MSCopyPipeLayout, MSCopyDescSetLayout, 32);
  CREATE_OBJECT(MSCopyPipe, MSCopyPipeLayout,
                driver->GetShaderCache()->GetBuiltinModule(BuiltinShader::PixelHistoryMSCopyCS));
  CREATE_OBJECT(MSCopyDepthPipe, MSCopyPipeLayout,
                driver->GetShaderCache()->GetBuiltinModule(BuiltinShader::PixelHistoryMSCopyDepthCS));
}

void VulkanReplay::PixelHistory::Destroy(WrappedVulkan *driver)
{
  if(MSCopyPipe != VK_NULL_HANDLE)
    driver->vkDestroyPipeline(driver->GetDev(), MSCopyPipe, NULL);
  if(MSCopyPipeLayout != VK_NULL_HANDLE)
    driver->vkDestroyPipelineLayout(driver->GetDev(), MSCopyPipeLayout, NULL);
  if(MSCopyDescSetLayout != VK_NULL_HANDLE)
    driver->vkDestroyDescriptorSetLayout(driver->GetDev(), MSCopyDescSetLayout, NULL);
  if(MSCopyDescPool != VK_NULL_HANDLE)
    driver->vkDestroyDescriptorPool(driver->GetDev(), MSCopyDescPool, NULL);
}

void VulkanReplay::HistogramMinMax::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  shaderCache->SetCaching(true);

  rdcstr glsl;

  CREATE_OBJECT(m_HistogramDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, VK_SHADER_STAGE_ALL, NULL},
                    {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  CREATE_OBJECT(m_HistogramPipeLayout, m_HistogramDescSetLayout, 0);

  for(size_t i = 0; i < ARRAY_COUNT(m_HistogramDescSet); i++)
    CREATE_OBJECT(m_HistogramDescSet[i], descriptorPool, m_HistogramDescSetLayout);

  rdcspv::CompilationSettings compileSettings;
  compileSettings.lang = rdcspv::InputLanguage::VulkanGLSL;
  compileSettings.stage = rdcspv::ShaderStage::Compute;

  // type max is one higher than the last RESTYPE, and RESTYPES are 1-indexed
  RDCCOMPILE_ASSERT(RESTYPE_TEXTYPEMAX == ARRAY_COUNT(m_MinMaxTilePipe),
                    "RESTYPE values don't match formats for dummy images");

  RDCCOMPILE_ASSERT(ARRAY_COUNT(m_MinMaxTilePipe) == arraydim<BuiltinShaderTextureType>(),
                    "Array size doesn't match parameter enum");
  RDCCOMPILE_ASSERT(ARRAY_COUNT(m_MinMaxTilePipe[0]) == arraydim<BuiltinShaderBaseType>(),
                    "Array size doesn't match parameter enum");

  for(BuiltinShaderTextureType t = BuiltinShaderTextureType::First;
      t < BuiltinShaderTextureType::Count; ++t)
  {
    for(BuiltinShaderBaseType f = BuiltinShaderBaseType::First; f < BuiltinShaderBaseType::Count; ++f)
    {
      CREATE_OBJECT(m_HistogramPipe[(size_t)t][(size_t)f], m_HistogramPipeLayout,
                    shaderCache->GetBuiltinModule(BuiltinShader::HistogramCS, f, t));
      CREATE_OBJECT(m_MinMaxTilePipe[(size_t)t][(size_t)f], m_HistogramPipeLayout,
                    shaderCache->GetBuiltinModule(BuiltinShader::MinMaxTileCS, f, t));

      if(t == BuiltinShaderTextureType::First)
      {
        CREATE_OBJECT(m_MinMaxResultPipe[(size_t)f], m_HistogramPipeLayout,
                      shaderCache->GetBuiltinModule(BuiltinShader::MinMaxResultCS, f));
      }
    }
  }

  shaderCache->SetCaching(false);

  const uint32_t maxTexDim = 16384;
  const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
  const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

  const size_t byteSize =
      2 * sizeof(Vec4f) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;

  m_MinMaxTileResult.Create(driver, driver->GetDev(), byteSize, 1,
                            GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
  m_MinMaxResult.Create(driver, driver->GetDev(), sizeof(Vec4f) * 2, 1,
                        GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
  m_MinMaxReadback.Create(driver, driver->GetDev(), sizeof(Vec4f) * 2, 1,
                          GPUBuffer::eGPUBufferReadback);
  m_HistogramBuf.Create(driver, driver->GetDev(), sizeof(uint32_t) * HGRAM_NUM_BUCKETS, 1,
                        GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
  m_HistogramReadback.Create(driver, driver->GetDev(), sizeof(uint32_t) * HGRAM_NUM_BUCKETS, 1,
                             GPUBuffer::eGPUBufferReadback);

  // don't need to ring this, as we hard-sync for readback anyway
  m_HistogramUBO.Create(driver, driver->GetDev(), sizeof(HistogramUBOData), 1, 0);
}

void VulkanReplay::HistogramMinMax::Destroy(WrappedVulkan *driver)
{
  if(m_HistogramDescSetLayout == VK_NULL_HANDLE)
    return;

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), m_HistogramDescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), m_HistogramPipeLayout, NULL);

  for(size_t t = 1; t < ARRAY_COUNT(m_MinMaxTilePipe); t++)
  {
    for(size_t f = 0; f < ARRAY_COUNT(m_MinMaxTilePipe[0]); f++)
    {
      driver->vkDestroyPipeline(driver->GetDev(), m_MinMaxTilePipe[t][f], NULL);
      driver->vkDestroyPipeline(driver->GetDev(), m_HistogramPipe[t][f], NULL);
      if(t == 1)
        driver->vkDestroyPipeline(driver->GetDev(), m_MinMaxResultPipe[f], NULL);
    }
  }

  m_MinMaxTileResult.Destroy();
  m_MinMaxResult.Destroy();
  m_MinMaxReadback.Destroy();
  m_HistogramBuf.Destroy();
  m_HistogramReadback.Destroy();
  m_HistogramUBO.Destroy();
}

void VulkanReplay::PostVS::Destroy(WrappedVulkan *driver)
{
  if(XFBQueryPool != VK_NULL_HANDLE)
    driver->vkDestroyQueryPool(driver->GetDev(), XFBQueryPool, NULL);
}

void VulkanReplay::Feedback::Destroy(WrappedVulkan *driver)
{
  FeedbackBuffer.Destroy();
}

void ShaderDebugData::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  // should match the enum ShaderDebugBind
  CREATE_OBJECT(
      DescSetLayout,
      {
          // ShaderDebugBind::Tex1D
          {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::Tex2D
          {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::Tex3D
          {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::Tex2DMS
          {4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::TexCube
          {5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::Buffer
          {6, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::Sampler
          {7, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
          // ShaderDebugBind::Constants
          {8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
      });

  CREATE_OBJECT(PipeLayout, DescSetLayout, sizeof(Vec4f) * 3 + sizeof(uint32_t));

  CREATE_OBJECT(DescSet, descriptorPool, DescSetLayout);

  VkResult vkr = VK_SUCCESS;

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
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0,
      NULL,
      VK_IMAGE_LAYOUT_UNDEFINED,
  };

  vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &Image);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};
  driver->vkGetImageMemoryRequirements(driver->GetDev(), Image, &mrq);

  // allocate readback memory
  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
      driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
  };

  vkr = driver->vkAllocateMemory(driver->GetDev(), &allocInfo, NULL, &ImageMemory);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = driver->vkBindImageMemory(driver->GetDev(), Image, ImageMemory, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      Image,
      VK_IMAGE_VIEW_TYPE_2D,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
      },
  };

  vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &ImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkAttachmentDescription attDesc = {
      0,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_CLEAR,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_GENERAL,
  };

  VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_GENERAL};

  VkSubpassDescription sub = {
      0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &attRef,
  };

  VkSubpassDependency deps[2] = {
      {
          VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_READ_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
      },
      {
          0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT, 0,
      },
  };

  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &attDesc, 1, &sub, 2, deps,
  };

  vkr = driver->vkCreateRenderPass(driver->GetDev(), &rpinfo, NULL, &RenderPass);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed to create shader debug render pass: %s", ToStr(vkr).c_str());

  // create framebuffer
  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, RenderPass, 1, &ImageView, 1, 1, 1,
  };

  vkr = driver->vkCreateFramebuffer(driver->GetDev(), &fbinfo, NULL, &Framebuffer);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // don't need to ring this, as we hard-sync for readback anyway
  ReadbackBuffer.Create(driver, driver->GetDev(), sizeof(Vec4f), 1, GPUBuffer::eGPUBufferReadback);
  ConstantsBuffer.Create(driver, driver->GetDev(), 1024, 1, 0);
}

void ShaderDebugData::Destroy(WrappedVulkan *driver)
{
  ConstantsBuffer.Destroy();
  ReadbackBuffer.Destroy();

  driver->vkDestroyPipeline(driver->GetDev(), MathPipe, NULL);

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), DescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), PipeLayout, NULL);

  driver->vkDestroyImage(driver->GetDev(), Image, NULL);
  driver->vkFreeMemory(driver->GetDev(), ImageMemory, NULL);
  driver->vkDestroyImageView(driver->GetDev(), ImageView, NULL);
  driver->vkDestroyFramebuffer(driver->GetDev(), Framebuffer, NULL);
  driver->vkDestroyRenderPass(driver->GetDev(), RenderPass, NULL);

  // one module each for float, uint, sint.
  driver->vkDestroyShaderModule(driver->GetDev(), Module[0], NULL);
  driver->vkDestroyShaderModule(driver->GetDev(), Module[1], NULL);
  driver->vkDestroyShaderModule(driver->GetDev(), Module[2], NULL);

  for(auto it = m_Pipelines.begin(); it != m_Pipelines.end(); it++)
    driver->vkDestroyPipeline(driver->GetDev(), it->second, NULL);
}
