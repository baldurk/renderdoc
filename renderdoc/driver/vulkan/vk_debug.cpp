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

#define AMD_GPUPERFAPI_SKIP_VULKAN_INCLUDE 1

#include "vk_debug.h"
#include <float.h>
#include "core/settings.h"
#include "data/glsl_shaders.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/amd/official/GPUPerfAPI/Include/gpu_perf_api_vk.h"
#include "driver/ihv/nv/nv_vk_counters.h"
#include "driver/shaders/spirv/spirv_compile.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

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
  if(setLayout != VK_NULL_HANDLE)
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

enum class StencilMode
{
  KEEP,
  KEEP_TEST_EQUAL_ONE,
  REPLACE,
  WRITE_ZERO,
};

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
  StencilMode stencilOperations;

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

  VkCompareOp stencilTest = VK_COMPARE_OP_ALWAYS;
  VkStencilOp stencilOp = VK_STENCIL_OP_KEEP;
  uint8_t stencilReference = 0;

  switch(info.stencilOperations)
  {
    case StencilMode::KEEP:
    {
      break;
    }
    case StencilMode::KEEP_TEST_EQUAL_ONE:
    {
      stencilTest = VK_COMPARE_OP_EQUAL;
      stencilReference = 1;
      break;
    }
    case StencilMode::REPLACE:
    {
      stencilOp = VK_STENCIL_OP_REPLACE;
      break;
    }
    case StencilMode::WRITE_ZERO:
    {
      stencilOp = VK_STENCIL_OP_ZERO;
      break;
    }
  };

  const VkPipelineDepthStencilStateCreateInfo depthStencil = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      NULL,
      0,
      info.depthEnable,
      info.depthEnable,
      VK_COMPARE_OP_ALWAYS,
      false,
      info.stencilEnable,
      {stencilOp, stencilOp, stencilOp, stencilTest, 0xff, 0xff, stencilReference},
      {stencilOp, stencilOp, stencilOp, stencilTest, 0xff, 0xff, stencilReference},
      0.0f,
      1.0f,
  };

  const VkPipelineColorBlendAttachmentState colAttach = {
      info.blendEnable,
      // colour blending
      info.srcBlend,
      info.dstBlend,
      VK_BLEND_OP_ADD,
      // alpha blending
      info.srcBlend,
      info.dstBlend,
      VK_BLEND_OP_ADD,
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
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(VulkanDebugManager));

  m_pDriver = driver;

  m_Device = m_pDriver->GetDev();
  VkDevice dev = m_Device;

  VulkanResourceManager *rm = driver->GetResourceManager();

  VkResult vkr = VK_SUCCESS;

  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  //////////////////////////////////////////////////////////////////
  // Color MS <-> Buffer copy (via compute)

  CREATE_OBJECT(m_BufferMSDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
                    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                    {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  rm->SetInternalResource(GetResID(m_BufferMSDescSetLayout));

  CREATE_OBJECT(m_BufferMSPipeLayout, m_BufferMSDescSetLayout, sizeof(Vec4u) * 2);

  rm->SetInternalResource(GetResID(m_BufferMSPipeLayout));

  CREATE_OBJECT(m_MS2BufferPipe, m_BufferMSPipeLayout,
                shaderCache->GetBuiltinModule(BuiltinShader::MS2BufferCS));
  CREATE_OBJECT(m_DepthMS2BufferPipe, m_BufferMSPipeLayout,
                shaderCache->GetBuiltinModule(BuiltinShader::DepthMS2BufferCS));
  CREATE_OBJECT(m_Buffer2MSPipe, m_BufferMSPipeLayout,
                shaderCache->GetBuiltinModule(BuiltinShader::Buffer2MSCS));

  rm->SetInternalResource(GetResID(m_MS2BufferPipe));
  rm->SetInternalResource(GetResID(m_DepthMS2BufferPipe));
  rm->SetInternalResource(GetResID(m_Buffer2MSPipe));

  //////////////////////////////////////////////////////////////////
  // Depth MS to Buffer copy (via compute)

  // need a dummy float formatted texture but that's easy as we can pick something guaranteed by the
  // spec
  {
    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        // format is required to be supported for sampling
        VK_FORMAT_R8G8B8A8_UNORM,
        {1, 1, 1},
        1,
        1,
        // sampledImageColorSampleCounts must include VK_SAMPLE_COUNT_4_BIT for 2D non-integer
        // optimal tiled textures
        VK_SAMPLE_COUNT_4_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &m_DummyDepthImage);
    CheckVkResult(vkr);

    NameVulkanObject(m_DummyDepthImage, "m_DummyDepthImage");

    rm->SetInternalResource(GetResID(m_DummyDepthImage));
  }

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

    vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &m_DummyStencilImage);
    CheckVkResult(vkr);

    NameVulkanObject(m_DummyStencilImage, "m_DummyStencilImage");

    rm->SetInternalResource(GetResID(m_DummyStencilImage));

    VkMemoryRequirements depthmrq = {};
    driver->vkGetImageMemoryRequirements(driver->GetDev(), m_DummyDepthImage, &depthmrq);

    VkMemoryRequirements mrq = {};
    driver->vkGetImageMemoryRequirements(driver->GetDev(), m_DummyStencilImage, &mrq);

    // assume we can combine these images into one allocation
    RDCASSERT((mrq.memoryTypeBits & depthmrq.memoryTypeBits) != 0, mrq.memoryTypeBits,
              depthmrq.memoryTypeBits);

    // only use memory types that support both
    mrq.memoryTypeBits &= depthmrq.memoryTypeBits;
    // use worst case alignment
    mrq.alignment = RDCMAX(mrq.alignment, depthmrq.alignment);

    // align each size individually (to worst case alignment)
    depthmrq.size = AlignUp(depthmrq.size, mrq.alignment);
    mrq.size = AlignUp(mrq.size, mrq.alignment);

    // allocate memory
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        depthmrq.size + mrq.size,
        driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = driver->vkAllocateMemory(driver->GetDev(), &allocInfo, NULL, &m_DummyMemory);
    CheckVkResult(vkr);

    if(vkr != VK_SUCCESS)
      return;

    rm->SetInternalResource(GetResID(m_DummyMemory));

    NameVulkanObject(m_DummyStencilImage, "m_DummyMemory");

    vkr = driver->vkBindImageMemory(driver->GetDev(), m_DummyStencilImage, m_DummyMemory, 0);
    CheckVkResult(vkr);

    vkr = driver->vkBindImageMemory(driver->GetDev(), m_DummyDepthImage, m_DummyMemory, mrq.size);
    CheckVkResult(vkr);

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        m_DummyStencilImage,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        f,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {
            viewAspectMask,
            0,
            1,
            0,
            1,
        },
    };

    vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &m_DummyStencilView);
    CheckVkResult(vkr);

    NameVulkanObject(m_DummyStencilView, "m_DummyStencilView");

    rm->SetInternalResource(GetResID(m_DummyStencilView));

    viewInfo.image = m_DummyDepthImage;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &m_DummyDepthView);
    CheckVkResult(vkr);

    NameVulkanObject(m_DummyDepthView, "m_DummyDepthView");

    rm->SetInternalResource(GetResID(m_DummyDepthView));

    VkCommandBuffer cmd = driver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

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
        Unwrap(m_DummyStencilImage),
        {barrierAspectMask, 0, 1, 0, 1},
    };

    DoPipelineBarrier(cmd, 1, &barrier);

    barrier.image = Unwrap(m_DummyDepthImage);
    barrierAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    DoPipelineBarrier(cmd, 1, &barrier);

    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

    break;
  }

  if(m_DummyStencilImage == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't find any integer format we could generate a dummy multisampled image with");
  }

  VkFormat formats[] = {
      VK_FORMAT_D16_UNORM,         VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32,
      VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT,        VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_S8_UINT,
  };

  VkSampleCountFlagBits sampleCounts[] = {
      VK_SAMPLE_COUNT_2_BIT,
      VK_SAMPLE_COUNT_4_BIT,
      VK_SAMPLE_COUNT_8_BIT,
      VK_SAMPLE_COUNT_16_BIT,
  };

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

    if(!m_pDriver->GetDeviceEnabledFeatures().sampleRateShading)
    {
      RDCDEBUG("No depth Array -> MSAA copies can be supported without sample rate shading");
      continue;
    }

    ConciseGraphicsPipeline depthPipeInfo = {
        VK_NULL_HANDLE,
        m_BufferMSPipeLayout,
        shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
        shaderCache->GetBuiltinModule(BuiltinShader::DepthBuf2MSFS),
        {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_STENCIL_REFERENCE},
        VK_SAMPLE_COUNT_1_BIT,
        true,    // sampleRateShading
        true,    // depthEnable
        true,    // stencilEnable
        StencilMode::REPLACE,
        false,    // colourOutput
        false,    // blendEnable
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ZERO,
        0xf,    // writeMask
    };

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

      depthPipeInfo.renderPass = depthArray2MSRP;
      depthPipeInfo.sampleCount = sampleCounts[s];

      CREATE_OBJECT(m_DepthArray2MSPipe[f][s], depthPipeInfo);

      rm->SetInternalResource(GetResID(m_DepthArray2MSPipe[f][s]));

      m_pDriver->vkDestroyRenderPass(dev, depthArray2MSRP, NULL);
    }
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    CREATE_OBJECT(m_DummyPipelineLayout, VK_NULL_HANDLE, 0);

    VkRenderPass SRGBA8RP = VK_NULL_HANDLE;

    CREATE_OBJECT(SRGBA8RP, VK_FORMAT_R8G8B8A8_SRGB);

    ConciseGraphicsPipeline dummyPipeInfo = {
        SRGBA8RP,
        m_DummyPipelineLayout,
        shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
        shaderCache->GetBuiltinModule(BuiltinShader::FixedColFS),
        {},
        VK_SAMPLE_COUNT_1_BIT,
        false,    // sampleRateShading
        false,    // depthEnable
        false,    // stencilEnable
        StencilMode::REPLACE,
        true,     // colourOutput
        false,    // blendEnable
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ZERO,
        0x0,    // writeMask
    };

    CREATE_OBJECT(m_DummyPipeline, dummyPipeInfo);

    driver->vkDestroyRenderPass(driver->GetDev(), SRGBA8RP, NULL);

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
    CheckVkResult(vkr);

    CREATE_OBJECT(m_DiscardSetLayout,
                  {
                      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
                  });

    CREATE_OBJECT(m_DiscardLayout, m_DiscardSetLayout, 4);

    ResourceFormat fmt;
    fmt.type = ResourceFormatType::Regular;
    fmt.compByteWidth = 4;
    fmt.compCount = 1;

    for(size_t i = 0; i < ARRAY_COUNT(m_DiscardSet); i++)
    {
      CREATE_OBJECT(m_DiscardSet[i], m_DiscardPool, m_DiscardSetLayout);

      fmt.compType = CompType::Float;
      bytebuf pattern = GetDiscardPattern(DiscardType(i), fmt);
      fmt.compType = CompType::UInt;
      pattern.append(GetDiscardPattern(DiscardType(i), fmt));

      m_DiscardCB[i].Create(m_pDriver, m_Device, pattern.size(), 1, 0);

      void *ptr = m_DiscardCB[i].Map();
      if(!ptr)
        return;
      memcpy(ptr, pattern.data(), pattern.size());
      m_DiscardCB[i].Unmap();

      VkDescriptorBufferInfo bufInfo = {};
      m_DiscardCB[i].FillDescriptor(bufInfo);

      VkWriteDescriptorSet writes[] = {
          {
              VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              NULL,
              Unwrap(m_DiscardSet[i]),
              0,
              0,
              1,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              NULL,
              &bufInfo,
              NULL,
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
}

VulkanDebugManager::~VulkanDebugManager()
{
  VkDevice dev = m_Device;

  m_Custom.Destroy(m_pDriver);

  m_ReadbackWindow.Destroy();

  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(uint32_t i = 0; i < VKMeshDisplayPipelines::ePipe_Count; i++)
      m_pDriver->vkDestroyPipeline(dev, it->second.pipes[i], NULL);

  for(VkDescriptorPool pool : m_BufferMSDescriptorPools)
    m_pDriver->vkDestroyDescriptorPool(dev, pool, NULL);

  m_pDriver->vkDestroyImageView(dev, m_DummyDepthView, NULL);
  m_pDriver->vkDestroyImage(dev, m_DummyDepthImage, NULL);
  m_pDriver->vkDestroyImageView(dev, m_DummyStencilView, NULL);
  m_pDriver->vkDestroyImage(dev, m_DummyStencilImage, NULL);
  m_pDriver->vkFreeMemory(dev, m_DummyMemory, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_BufferMSDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_BufferMSPipeLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_Buffer2MSPipe, NULL);

  m_pDriver->vkDestroyPipeline(dev, m_MS2BufferPipe, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_DepthMS2BufferPipe, NULL);

  m_pDriver->vkDestroyPipelineLayout(dev, m_DummyPipelineLayout, NULL);
  m_pDriver->vkDestroyPipeline(dev, m_DummyPipeline, NULL);

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
    for(size_t i = 0; i < ARRAY_COUNT(it->second.pso); i++)
      m_pDriver->vkDestroyPipeline(dev, it->second.pso[i], NULL);
    m_pDriver->vkDestroyRenderPass(dev, it->second.rp, NULL);
  }

  for(auto it = m_DiscardPatterns.begin(); it != m_DiscardPatterns.end(); it++)
    m_pDriver->vkDestroyBuffer(dev, it->second, NULL);
  for(auto it = m_DiscardStage.begin(); it != m_DiscardStage.end(); it++)
    it->second.Destroy();

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
      m_pDriver->vkDestroyFramebuffer(dev, m_Custom.TexFB, NULL);

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
      CheckVkResult(vkr);
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
  CheckVkResult(vkr);

  NameVulkanObject(m_Custom.TexImg, "m_Custom.TexImg");

  VkMemoryRequirements mrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(m_Device, m_Custom.TexImg, &mrq);

  // if no memory is allocated, or it's not enough,
  // then allocate
  if(m_Custom.TexMem == VK_NULL_HANDLE || mrq.size > m_Custom.TexMemSize)
  {
    if(m_Custom.TexMem != VK_NULL_HANDLE)
      m_pDriver->vkFreeMemory(m_Device, m_Custom.TexMem, NULL);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &m_Custom.TexMem);
    CheckVkResult(vkr);

    if(vkr != VK_SUCCESS)
      return;

    m_Custom.TexMemSize = mrq.size;
  }

  vkr = m_pDriver->vkBindImageMemory(m_Device, m_Custom.TexImg, m_Custom.TexMem, 0);
  CheckVkResult(vkr);

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
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          1,
          0,
          1,
      },
  };

  for(uint32_t i = 0; i < imInfo.mipLevels; i++)
  {
    viewInfo.subresourceRange.baseMipLevel = i;
    vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &m_Custom.TexImgView[i]);
    CheckVkResult(vkr);

    NameVulkanObject(m_Custom.TexImgView[i], "m_Custom.TexImgView[" + ToStr(i) + "]");
  }

  // need to update image layout into valid state

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  m_pDriver->FindImageState(GetResID(m_Custom.TexImg))
      ->InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         m_pDriver->GetImageTransitionInfo());

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  CheckVkResult(vkr);

  if(Vulkan_Debug_SingleSubmitFlushing())
    m_pDriver->SubmitCmds();

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
  CheckVkResult(vkr);
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
      StencilMode::KEEP,
      true,     // colourOutput
      false,    // blendEnable
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      0xf,    // writeMask
  };

  CREATE_OBJECT(m_Custom.TexPipeline, customPipe);
}

uint32_t VulkanReplay::PickVertex(uint32_t eventId, int32_t width, int32_t height,
                                  const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  VkMarkerRegion::Begin(StringFormat::Fmt("VulkanReplay::PickVertex(%u, %u)", x, y));

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(width) / float(height));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f pickMVP = projMat.Mul(camMat);
  if(!cfg.position.unproject)
  {
    pickMVP = pickMVP.Mul(Matrix4f(cfg.axisMapping));
  }

  bool reverseProjection = false;
  Matrix4f guessProj;
  Matrix4f guessProjInverse;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    if(cfg.position.farPlane != FLT_MAX)
    {
      guessProj =
          Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect);
    }
    else
    {
      reverseProjection = true;
      guessProj = Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);
    }

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    if(cfg.position.flipY)
      guessProj[5] *= -1.0f;

    guessProjInverse = guessProj.Inverse();
  }

  Vec3f rayPos;
  Vec3f rayDir;
  // convert mouse pos to world space ray
  {
    float pickX = ((float)x) / ((float)width);
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)height);
    // flip the Y axis by default for Y-up
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    if(cfg.position.flipY && !cfg.ortho)
      pickYCanonical = -pickYCanonical;

    // x/y is inside the window. Since we're not using the window projection we need to correct
    // for the aspect ratio here.
    if(cfg.position.unproject && !cfg.ortho)
      pickXCanonical *= (float(width) / float(height)) / cfg.aspect;

    // set up the NDC near/far pos
    Vec3f nearPosNDC = Vec3f(pickXCanonical, pickYCanonical, 0);
    Vec3f farPosNDC = Vec3f(pickXCanonical, pickYCanonical, 1);

    if(cfg.position.unproject && cfg.ortho)
    {
      // orthographic projections we raycast in NDC space
      Matrix4f inversePickMVP = pickMVP.Inverse();

      // transform from the desired NDC co-ordinates into camera space
      Vec3f nearPosCamera = inversePickMVP.Transform(nearPosNDC, 1);
      Vec3f farPosCamera = inversePickMVP.Transform(farPosNDC, 1);

      Vec3f testDir = (farPosCamera - nearPosCamera);
      testDir.Normalise();

      Matrix4f pickMVPguessProjInverse = guessProj.Mul(inversePickMVP);

      Vec3f nearPosProj = pickMVPguessProjInverse.Transform(nearPosNDC, 1);
      Vec3f farPosProj = pickMVPguessProjInverse.Transform(farPosNDC, 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      // Calculate the ray direction first in the regular way (above), so we can use the
      // the output for testing if the ray we are picking is negative or not. This is similar
      // to checking against the forward direction of the camera, but more robust
      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else if(cfg.position.unproject)
    {
      // projected data we pick in world-space to avoid problems with handling unusual transforms

      if(reverseProjection)
      {
        farPosNDC.z = 1e-6f;
        nearPosNDC.z = 1e+6f;
      }

      // invert the guessed projection matrix to get the near/far pos in camera space
      Vec3f nearPosCamera = guessProjInverse.Transform(nearPosNDC, 1.0f);
      Vec3f farPosCamera = guessProjInverse.Transform(farPosNDC, 1.0f);

      // normalise and generate the ray
      rayDir = (farPosCamera - nearPosCamera);
      rayDir.Normalise();

      farPosCamera = nearPosCamera + rayDir;

      // invert the camera transform to transform the ray as camera-relative into world space
      Matrix4f inverseCamera = camMat.Inverse();

      Vec3f nearPosWorld = inverseCamera.Transform(nearPosCamera, 1);
      Vec3f farPosWorld = inverseCamera.Transform(farPosCamera, 1);

      // again normalise our final ray
      rayDir = (farPosWorld - nearPosWorld);
      rayDir.Normalise();

      rayPos = nearPosWorld;
    }
    else
    {
      Matrix4f inversePickMVP = pickMVP.Inverse();

      // transform from the desired NDC co-ordinates into model space
      Vec3f nearPosCamera = inversePickMVP.Transform(nearPosNDC, 1);
      Vec3f farPosCamera = inversePickMVP.Transform(farPosNDC, 1);

      rayDir = (farPosCamera - nearPosCamera);
      rayDir.Normalise();
      rayPos = nearPosCamera;
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
    if(!mappedPtr)
      return ~0U;

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
  else
  {
    // ensure IB is non-empty so we have a valid descriptor below
    if(m_VertexPick.IBSize == 0)
    {
      m_VertexPick.IBSize = 1 * sizeof(uint32_t);

      m_VertexPick.IB.Create(m_pDriver, dev, m_VertexPick.IBSize, 1,
                             GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
    }
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
    if(!vbData)
      return ~0U;

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
  if(!ubo)
    return ~0U;

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
  ubo->flipY = cfg.position.flipY;
  ubo->ortho = cfg.ortho;
  ubo->coords = Vec2f((float)x, (float)y);
  ubo->viewport = Vec2f((float)width, (float)height);

  if(cfg.position.unproject && isTriangleMesh)
  {
    // projected triangle meshes we transform the vertices into world space, and ray-cast against
    // that
    //
    // NOTE: for ortho, this matrix is not used and we just do the perspective W division on model
    // vertices. The ray is cast in NDC
    if(cfg.ortho)
      ubo->transformMat = Matrix4f::Identity();
    else
      ubo->transformMat = guessProjInverse;
  }
  else if(cfg.position.unproject)
  {
    // projected non-triangles are just point clouds, so we transform the vertices into world space
    // then project them back onto the output and compare that against the picking 2D co-ordinates
    ubo->transformMat = pickMVP.Mul(guessProjInverse);
  }
  else
  {
    // plain meshes of either type, we just transform from model space to the output, and raycast or
    // co-ordinate check
    ubo->transformMat = pickMVP;
  }

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

  vt->UpdateDescriptorSets(Unwrap(m_Device), 2, writes, 0, NULL);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return ~0U;

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
  CheckVkResult(vkr);

  if(Vulkan_Debug_SingleSubmitFlushing())
    m_pDriver->SubmitCmds();

  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  uint32_t *pickResultData = (uint32_t *)m_VertexPick.ResultReadback.Map();
  uint32_t numResults = *pickResultData;
  if(!pickResultData)
    return ~0U;

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

VkDescriptorSet VulkanDebugManager::GetBufferMSDescSet()
{
  if(m_FreeBufferMSDescriptorSets.empty())
  {
    WrappedVulkan *driver = m_pDriver;
    VulkanResourceManager *rm = driver->GetResourceManager();
    VkResult vkr = VK_SUCCESS;
    VkDevice dev = m_pDriver->GetDev();

    VkDescriptorPoolSize bufferPoolTypes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 * BufferMSDescriptorPoolSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * BufferMSDescriptorPoolSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * BufferMSDescriptorPoolSize},
    };

    VkDescriptorPoolCreateInfo bufferPoolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        NULL,
        0,
        BufferMSDescriptorPoolSize,
        ARRAY_COUNT(bufferPoolTypes),
        &bufferPoolTypes[0],
    };

    VkDescriptorPool pool;
    vkr = m_pDriver->vkCreateDescriptorPool(dev, &bufferPoolInfo, NULL, &pool);
    CheckVkResult(vkr);

    rm->SetInternalResource(GetResID(pool));

    m_BufferMSDescriptorPools.push_back(pool);

    rdcarray<VkDescriptorSetLayout> setLayouts;
    setLayouts.fill(BufferMSDescriptorPoolSize, m_BufferMSDescSetLayout);

    VkDescriptorSetAllocateInfo descSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        NULL,
        pool,
        (uint32_t)setLayouts.size(),
        setLayouts.data(),
    };

    m_FreeBufferMSDescriptorSets.resize(BufferMSDescriptorPoolSize);
    m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, m_FreeBufferMSDescriptorSets.data());

    for(VkDescriptorSet set : m_FreeBufferMSDescriptorSets)
      rm->SetInternalResource(GetResID(set));
  }

  VkDescriptorSet ret = m_FreeBufferMSDescriptorSets.back();
  m_FreeBufferMSDescriptorSets.pop_back();
  m_UsedBufferMSDescriptorSets.push_back(ret);
  return ret;
}

void VulkanDebugManager::ResetBufferMSDescriptorPools()
{
  m_FreeBufferMSDescriptorSets.append(m_UsedBufferMSDescriptorSets);
  m_UsedBufferMSDescriptorSets.clear();
}

void VulkanDebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  if(!m_pDriver->GetResourceManager()->HasCurrentResource(buff))
  {
    RDCERR("Getting buffer data for unknown buffer/memory %s!", ToStr(buff).c_str());
    return;
  }

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
    bufsize = m_pDriver->m_CreationInfo.m_Memory[buff].wholeMemBufSize;

    if(srcBuf == VK_NULL_HANDLE)
    {
      RDCLOG(
          "Memory doesn't have wholeMemBuf, either non-buffer accessible (non-linear) or dedicated "
          "image memory");
      return;
    }
  }
  else if(WrappedVkBuffer::IsAlloc(res))
  {
    srcBuf = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(buff);
    bufsize = m_pDriver->m_CreationInfo.m_Buffer[buff].size;
  }
  else
  {
    RDCERR("Getting buffer data for object that isn't buffer or memory %s!", ToStr(buff).c_str());
    return;
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

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

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
  CheckVkResult(vkr);

  if(Vulkan_Debug_SingleSubmitFlushing())
    m_pDriver->SubmitCmds();

  while(sizeRemaining > 0)
  {
    VkDeviceSize chunkSize = RDCMIN(sizeRemaining, STAGE_BUFFER_BYTE_SIZE);

    cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

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
    CheckVkResult(vkr);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    byte *pData = NULL;
    vkr = vt->MapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem), 0, VK_WHOLE_SIZE, 0,
                        (void **)&pData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS)
      return;
    if(!pData)
    {
      RDCERR("Manually reporting failed memory map");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      return;
    }

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, Unwrap(m_ReadbackWindow.mem), 0, VK_WHOLE_SIZE,
    };

    vkr = vt->InvalidateMappedMemoryRanges(Unwrap(dev), 1, &range);
    CheckVkResult(vkr);

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

  const VkImageAspectFlags imAspects = FormatImageAspects(imInfo.format);

  VkImageSubresourceRange barrierDiscardRange = discardRange;
  barrierDiscardRange.aspectMask = imAspects;

  if(imInfo.samples > 1)
  {
    WrappedVulkan *driver = m_pDriver;

    bool depth = false;
    if(IsDepthOrStencilFormat(imInfo.format))
      depth = true;

    rdcpair<VkFormat, VkSampleCountFlagBits> key = {imInfo.format, imInfo.samples};

    DiscardPassData &passdata = m_DiscardPipes[key];

    // create and cache a pipeline and RP that writes to this format and sample count
    if(passdata.pso[0] == VK_NULL_HANDLE)
    {
      BuiltinShaderBaseType baseType = BuiltinShaderBaseType::Float;

      if(IsSIntFormat(imInfo.format))
        baseType = BuiltinShaderBaseType::SInt;
      else if(IsUIntFormat(imInfo.format))
        baseType = BuiltinShaderBaseType::UInt;

      VkAttachmentReference attRef = {
          0,
          depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
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
          0,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
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
          m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::DiscardFS, baseType),
          {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE},
          imInfo.samples,
          false,    // sampleRateShading
          true,     // depthEnable
          true,     // stencilEnable
          StencilMode::REPLACE,
          true,     // colourOutput
          false,    // blendEnable
          VK_BLEND_FACTOR_ONE,
          VK_BLEND_FACTOR_ZERO,
          0xf,    // writeMask
      };

      CREATE_OBJECT(passdata.pso[0], pipeInfo);

      if(depth)
      {
        // depth-only, no stencil
        pipeInfo.stencilEnable = false;
        pipeInfo.stencilOperations = StencilMode::KEEP;

        CREATE_OBJECT(passdata.pso[1], pipeInfo);

        // stencil-only, no depth
        pipeInfo.depthEnable = false;
        pipeInfo.stencilEnable = true;
        pipeInfo.stencilOperations = StencilMode::REPLACE;

        CREATE_OBJECT(passdata.pso[2], pipeInfo);
      }
    }

    if(passdata.pso[0] == VK_NULL_HANDLE)
      return;

    DiscardImgData &imgdata = m_DiscardImages[GetResID(image)];

    // create and cache views and framebuffers for every slice in this image
    if(imgdata.fbs.empty())
    {
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
                imAspects,
                0,
                1,
                a,
                1,
            },
        };

        VkImageView view;
        VkResult vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &view);
        CheckVkResult(vkr);
        NameVulkanObject(view, StringFormat::Fmt("FillWithDiscardPattern view %s",
                                                 ToStr(GetResID(image)).c_str()));

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
        CheckVkResult(vkr);

        imgdata.fbs.push_back(fb);
      }
    }

    if(imgdata.fbs.empty())
      return;

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(passdata.pso[0]));
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
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_ALL_READ_BITS | VK_ACCESS_ALL_WRITE_BITS,
        depth ? (VkAccessFlags)VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
              : (VkAccessFlags)VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        curLayout,
        depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
              : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(image),
        barrierDiscardRange,
    };

    DoPipelineBarrier(cmd, 1, &dstimBarrier);

    ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DiscardLayout), VK_SHADER_STAGE_ALL, 0, 4,
                                   &pass);

    if(imAspects != discardRange.aspectMask &&
       imAspects == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
    {
      // if we're only discarding one of depth or stencil in a depth/stencil image, pick a
      // framebuffer that only targets that aspect.
      if(discardRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
      {
        if(passdata.pso[1] == VK_NULL_HANDLE)
        {
          RDCERR("Don't have depth-only pipeline for masking out stencil discard");
          return;
        }

        ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Unwrap(passdata.pso[1]));
      }
      else
      {
        if(passdata.pso[2] == VK_NULL_HANDLE)
        {
          RDCERR("Don't have stencil-only pipeline for masking out depth discard");
          return;
        }

        ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Unwrap(passdata.pso[2]));
      }
    }

    for(uint32_t slice = discardRange.baseArrayLayer;
        slice < discardRange.baseArrayLayer + discardRange.layerCount; slice++)
    {
      rpbegin.framebuffer = Unwrap(imgdata.fbs[slice]);
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
        ObjDisp(cmd)->CmdSetStencilReference(
            Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT | VK_STENCIL_FACE_BACK_BIT, 0x00);
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }

      ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
    }

    dstimBarrier.oldLayout = dstimBarrier.newLayout;
    dstimBarrier.newLayout = curLayout;
    dstimBarrier.srcAccessMask = depth ? (VkAccessFlags)VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                       : (VkAccessFlags)VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dstimBarrier.dstAccessMask = VK_ACCESS_ALL_WRITE_BITS | VK_ACCESS_ALL_READ_BITS;

    DoPipelineBarrier(cmd, 1, &dstimBarrier);

    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindInitial,
                                                false);

    return;
  }

  rdcpair<VkFormat, DiscardType> key = {imInfo.format, type};

  if(key.first == VK_FORMAT_X8_D24_UNORM_PACK32)
    key.first = VK_FORMAT_D24_UNORM_S8_UINT;
  if(key.first == VK_FORMAT_S8_UINT)
    key.first = VK_FORMAT_D32_SFLOAT_S8_UINT;

  VkBuffer buf = m_DiscardPatterns[key];
  VkResult vkr = VK_SUCCESS;

  static const uint32_t PatternBatchWidth = 256;
  static const uint32_t PatternBatchHeight = 256;

  VkImageAspectFlags aspectFlags = discardRange.aspectMask & FormatImageAspects(imInfo.format);

  if(buf == VK_NULL_HANDLE)
  {
    GPUBuffer &stage = m_DiscardStage[key];
    bytebuf pattern = GetDiscardPattern(key.second, MakeResourceFormat(key.first));

    BlockShape shape = GetBlockShape(key.first, 0);

    if(key.first == VK_FORMAT_D32_SFLOAT_S8_UINT)
      shape = {1, 1, 4};

    stage.Create(m_pDriver, m_Device, pattern.size(), 1, 0);

    void *ptr = stage.Map();
    if(!ptr)
      return;
    memcpy(ptr, pattern.data(), pattern.size());
    stage.Unmap();

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        pattern.size() * (PatternBatchWidth / DiscardPatternWidth) *
            (PatternBatchHeight / DiscardPatternHeight),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &buf);
    CheckVkResult(vkr);

    MemoryAllocation alloc = m_pDriver->AllocateMemoryForResource(
        buf, MemoryScope::ImmutableReplayDebug, MemoryType::GPULocal);

    if(alloc.mem == VK_NULL_HANDLE)
      return;

    vkr = vt->BindBufferMemory(Unwrap(dev), Unwrap(buf), Unwrap(alloc.mem), alloc.offs);
    CheckVkResult(vkr);

    rdcarray<VkBufferCopy> bufRegions;
    VkBufferCopy bufCopy;
    // copy one row at a time (row of blocks, for blocks)
    bufCopy.size = shape.bytes * DiscardPatternWidth / shape.width;

    const uint32_t numHorizBatches = PatternBatchWidth / DiscardPatternWidth;

    for(uint32_t y = 0; y < PatternBatchHeight / shape.height; y++)
    {
      // copy from the rows sequentially
      bufCopy.srcOffset = bufCopy.size * (y % (DiscardPatternHeight / shape.height));
      for(uint32_t x = 0; x < numHorizBatches; x++)
      {
        bufCopy.dstOffset = y * bufCopy.size * numHorizBatches + x * bufCopy.size;
        bufRegions.push_back(bufCopy);
      }
    }

    // copy byte-packed second stencil pattern afterwards
    if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
      const VkDeviceSize srcDepthOffset = DiscardPatternWidth * DiscardPatternHeight * shape.bytes;
      const VkDeviceSize dstDepthOffset = PatternBatchWidth * PatternBatchHeight * shape.bytes;

      bufCopy.size = DiscardPatternWidth;
      for(uint32_t y = 0; y < PatternBatchHeight; y++)
      {
        // copy from the rows sequentially. The source data is depth-pitched
        bufCopy.srcOffset = srcDepthOffset + bufCopy.size * shape.bytes * (y % DiscardPatternHeight);
        for(uint32_t x = 0; x < numHorizBatches; x++)
        {
          bufCopy.dstOffset = dstDepthOffset + y * bufCopy.size * numHorizBatches + x * bufCopy.size;
          bufRegions.push_back(bufCopy);
        }
      }
    }

    vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(stage.buf), Unwrap(buf), (uint32_t)bufRegions.size(),
                      bufRegions.data());

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
          for(uint32_t y = discardRect.offset.y; y < extent.height; y += PatternBatchHeight)
          {
            for(uint32_t x = discardRect.offset.x; x < extent.width; x += PatternBatchWidth)
            {
              VkBufferImageCopy region = {
                  0,
                  0,
                  0,
                  {aspectFlags, m, a, 1},
                  {
                      (int)x,
                      (int)y,
                      (int)z,
                  },
              };

              region.imageExtent.width = RDCMIN(PatternBatchWidth, extent.width - x);
              region.imageExtent.height = RDCMIN(PatternBatchHeight, extent.height - y);
              region.imageExtent.depth = 1;

              region.bufferRowLength = PatternBatchWidth;

              // for depth/stencil copies, write depth first
              if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

              if(aspectFlags != VK_IMAGE_ASPECT_STENCIL_BIT)
                mainCopies.push_back(region);

              if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
              {
                uint32_t depthStride = (imInfo.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2 : 4);
                VkDeviceSize depthOffset = PatternBatchWidth * PatternBatchHeight * depthStride;

                // if it's a depth/stencil format, write stencil separately
                region.bufferOffset = depthOffset;
                region.bufferRowLength = PatternBatchWidth;
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
      barrierDiscardRange,
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

void VulkanDebugManager::InitReadbackBuffer(VkDeviceSize sz)
{
  if(m_ReadbackWindow.buf == VK_NULL_HANDLE || m_ReadbackWindow.sz < sz)
  {
    if(m_ReadbackWindow.buf != VK_NULL_HANDLE)
    {
      m_ReadbackWindow.Destroy();
    }

    VkDevice dev = m_pDriver->GetDev();
    m_ReadbackWindow.Create(m_pDriver, dev, AlignUp(sz, (VkDeviceSize)4096), 1,
                            GPUBuffer::eGPUBufferReadback);

    RDCLOG("Allocating readback window of %llu bytes", m_ReadbackWindow.sz);

    VkResult vkr = ObjDisp(dev)->MapMemory(Unwrap(dev), Unwrap(m_ReadbackWindow.mem), 0,
                                           VK_WHOLE_SIZE, 0, (void **)&m_ReadbackPtr);
    CheckVkResult(vkr);
    if(!m_ReadbackPtr)
    {
      RDCERR("Manually reporting failed memory map");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
    }
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

  struct AllocedWrites
  {
    ~AllocedWrites()
    {
      for(VkDescriptorBufferInfo *a : bufWrites)
        delete[] a;
      for(VkWriteDescriptorSetInlineUniformBlock *a : inlineWrites)
        delete a;
    }

    rdcarray<VkDescriptorBufferInfo *> bufWrites;
    rdcarray<VkWriteDescriptorSetInlineUniformBlock *> inlineWrites;
  } alloced;

  rdcarray<VkDescriptorBufferInfo *> &allocBufWrites = alloced.bufWrites;

  rdcarray<VkWriteDescriptorSetInlineUniformBlock *> &allocInlineWrites = alloced.inlineWrites;

  // one for each descriptor type. 1 of each to start with, we then increment for each descriptor
  // we need to allocate
  rdcarray<VkDescriptorPoolSize> poolSizes = {
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
      {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 0},
  };

  // array of descriptor types, used for generating lists for binding data. Each unique bitmask
  // will have an offset (see below) pointing into this array where that bitmask's list of
  // descriptors is
  rdcarray<VkDescriptorType> mutableTypeArray;

  // array of unique bitmasks encountered
  rdcarray<uint64_t> mutablePoolsizeBitmask;
  // parallel array to mutablePoolsizeBitmask with the [offset,range] in mutableTypeArray where the
  // bitmask's type list is.
  rdcarray<rdcpair<size_t, uint32_t>> mutableBitmaskArrayRange;

  // populate mutable bitmasks. This loop is the same as the one below which is more commented
  for(size_t i = 0; i < setLayouts.size(); i++)
  {
    if(i < pipeInfo.descSetLayouts.size() && i < pipe.descSets.size() &&
       pipe.descSets[i].pipeLayout != ResourceId())
    {
      const VulkanCreationInfo::PipelineLayout &pipelineLayoutInfo =
          creationInfo.m_PipelineLayout[pipe.descSets[i].pipeLayout];

      if(pipelineLayoutInfo.descSetLayouts[i] == ResourceId())
        continue;

      const DescSetLayout &origLayout =
          creationInfo.m_DescSetLayout[pipelineLayoutInfo.descSetLayouts[i]];

      for(size_t b = 0; b < origLayout.bindings.size(); b++)
      {
        uint64_t mutableBitmask = origLayout.mutableBitmasks[b];

        int bitmaskIdx = mutablePoolsizeBitmask.indexOf(mutableBitmask);
        if(bitmaskIdx == -1)
        {
          bitmaskIdx = mutablePoolsizeBitmask.count();
          mutablePoolsizeBitmask.push_back(mutableBitmask);
          poolSizes.push_back({VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 0});

          uint32_t count = 0;
          for(uint64_t m = 0; m < 64; m++)
          {
            if(((1ULL << m) & mutableBitmask) == 0)
              continue;

            mutableTypeArray.push_back(convert(DescriptorSlotType(m)));
            count++;
          }
          mutableBitmaskArrayRange.push_back({mutableTypeArray.size() - count, count});
        }
      }
    }
  }

  VkDescriptorPoolInlineUniformBlockCreateInfo inlineCreateInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,
  };

  static const uint32_t NormalDescriptorCount = 11;
  static const uint32_t InlinePoolIndex = 11;
  static const uint32_t MutablePoolStart = 12;

  uint32_t poolSizeCount = NormalDescriptorCount;

  // count up our own
  for(size_t i = 0; i < newBindingsCount; i++)
  {
    RDCASSERT((uint32_t)newBindings[i].descriptorType < NormalDescriptorCount,
              newBindings[i].descriptorType);
    poolSizes[newBindings[i].descriptorType].descriptorCount += newBindings[i].descriptorCount;
  }

  VkMutableDescriptorTypeCreateInfoEXT mutableCreateInfo = {
      VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
  };

  // need to add our added bindings to the first descriptor set
  rdcarray<VkDescriptorSetLayoutBinding> bindings(newBindings, newBindingsCount);
  // this is a per-bindings array, only used for mutable descriptors
  rdcarray<VkMutableDescriptorTypeListEXT> mutableTypeLists;

  // if there are fewer sets bound than were declared in the pipeline layout, only process the
  // bound sets (as otherwise we'd fail to copy from them). Assume the application knew what it
  // was doing and the other sets are statically unused.
  setLayouts.resize(RDCMIN(pipe.descSets.size(), pipeInfo.descSetLayouts.size()));

  size_t boundDescs = setLayouts.size();

  // need at least one set, if the shader isn't using any we'll just make our own
  if(setLayouts.empty())
    setLayouts.resize(1);

  // start with the limits as they are, and subtract off them incrementally. When any limit would
  // drop below 0, we fail.
  uint32_t maxPerStageDescriptorSamplers[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSamplers,
  };
  uint32_t maxPerStageDescriptorUniformBuffers[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorUniformBuffers,
  };
  uint32_t maxPerStageDescriptorStorageBuffers[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers,
  };
  uint32_t maxPerStageDescriptorSampledImages[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorSampledImages,
  };
  uint32_t maxPerStageDescriptorStorageImages[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageImages,
  };
  uint32_t maxPerStageDescriptorInputAttachments[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
      m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorInputAttachments,
  };
  uint32_t maxPerStageResources[NumShaderStages] = {
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
      m_pDriver->GetDeviceProps().limits.maxPerStageResources,
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
  uint32_t maxPerStageDescriptorInlineUniformBlocks[NumShaderStages] = {};

  if(m_pDriver->GetExtensions(NULL).ext_EXT_inline_uniform_block)
  {
    VkPhysicalDeviceInlineUniformBlockProperties inlineProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES,
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
    for(uint32_t sbit = 0; sbit < NumShaderStages; sbit++)                                     \
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

    // clear any mutable type lists
    mutableTypeLists.clear();

    // if the shader had no descriptor sets at all, i will be invalid, so just skip and add a set
    // with only our own bindings.
    if(i < pipeInfo.descSetLayouts.size() && i < pipe.descSets.size() &&
       pipe.descSets[i].pipeLayout != ResourceId())
    {
      const VulkanCreationInfo::PipelineLayout &pipelineLayoutInfo =
          creationInfo.m_PipelineLayout[pipe.descSets[i].pipeLayout];

      if(pipelineLayoutInfo.descSetLayouts[i] == ResourceId())
        continue;

      // use the descriptor set layout from when it was bound. If the pipeline layout declared a
      // descriptor set layout for this set, but it's statically unused, it may be complete
      // garbage and doesn't match what the shader uses. However the pipeline layout at descriptor
      // set bind time must have been compatible and valid so we can use it. If this set *is* used
      // then the pipeline layout at bind time must be compatible with the pipeline's pipeline
      // layout, so we're fine too.
      const DescSetLayout &origLayout =
          creationInfo.m_DescSetLayout[pipelineLayoutInfo.descSetLayouts[i]];

      WrappedVulkan::DescriptorSetInfo &setInfo =
          m_pDriver->m_DescriptorSetState[pipe.descSets[i].descSet];

      for(size_t b = 0; !error && b < origLayout.bindings.size(); b++)
      {
        const DescSetLayout::Binding &layoutBind = origLayout.bindings[b];

        // skip empty bindings
        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          continue;

        uint32_t descriptorCount = layoutBind.descriptorCount;

        if(layoutBind.variableSize)
          descriptorCount = setInfo.data.variableDescriptorCount;

        // make room in the pool
        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          poolSizes[InlinePoolIndex].descriptorCount += descriptorCount;
          inlineCreateInfo.maxInlineUniformBlockBindings++;
        }
        else if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
        {
          int bitmaskIdx = mutablePoolsizeBitmask.indexOf(origLayout.mutableBitmasks[b]);
          RDCASSERT(bitmaskIdx >= 0);
          poolSizes[MutablePoolStart + bitmaskIdx].descriptorCount += descriptorCount;

          // each mutable descriptor needs a type list
          mutableTypeLists.resize_for_index(b);
          mutableTypeLists[b].descriptorTypeCount = mutableBitmaskArrayRange[bitmaskIdx].second;
          mutableTypeLists[b].pDescriptorTypes =
              mutableTypeArray.data() + mutableBitmaskArrayRange[bitmaskIdx].first;
        }
        else
        {
          poolSizes[layoutBind.layoutDescType].descriptorCount += descriptorCount;
        }

        VkDescriptorSetLayoutBinding newBind;
        // offset the binding. We offset all sets to make it easier for patching - don't need to
        // conditionally patch shader bindings depending on which set they're in.
        newBind.binding = uint32_t(b + newBindingsCount);
        newBind.descriptorCount = descriptorCount;
        newBind.descriptorType = layoutBind.layoutDescType;

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
          newBind.stageFlags = layoutBind.stageFlags;

        // mutable descriptors count against all limits they can be used against. This loop will
        // only execute for mutable descriptors, others will just execute once using their real type
        for(uint64_t m = 0; m < 64; m++)
        {
          VkDescriptorType descType = layoutBind.layoutDescType;

          if(descType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
          {
            // if this type's bit isn't set in the bitmask of available descriptors then continue
            if(((1ULL << m) & origLayout.mutableBitmasks[b]) == 0)
              continue;

            // this type is allowed, convert it to an enum and check it against the limits below
            descType = convert(DescriptorSlotType(m));
          }

          switch(descType)
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
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
              descriptorCount = 1;
              UPDATE_AND_CHECK_LIMIT(maxDescriptorSetInlineUniformBlocks);
              UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageDescriptorInlineUniformBlocks);
              break;
            default: break;
          }

          // we're only looping for mutables
          if(layoutBind.layoutDescType != VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
            break;
        }

        UPDATE_AND_CHECK_STAGE_LIMIT(maxPerStageResources);

        if(layoutBind.immutableSampler)
        {
          hasImmutableSamplers = true;
          VkSampler *samplers = new VkSampler[layoutBind.descriptorCount];
          newBind.pImmutableSamplers = samplers;
          for(uint32_t s = 0; s < layoutBind.descriptorCount; s++)
            samplers[s] =
                GetResourceManager()->GetCurrentHandle<VkSampler>(layoutBind.immutableSampler[s]);
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

    if(!mutableTypeLists.empty())
    {
      descsetLayoutInfo.pNext = &mutableCreateInfo;
      mutableCreateInfo.mutableDescriptorTypeListCount = (uint32_t)mutableTypeLists.size();
      mutableCreateInfo.pMutableDescriptorTypeLists = mutableTypeLists.data();
    }

    if(!error)
    {
      // create new offseted descriptor layout
      vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL, &setLayouts[i]);
      CheckVkResult(vkr);
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
  poolCreateInfo.pPoolSizes = poolSizes.data();

  if(inlineCreateInfo.maxInlineUniformBlockBindings > 0)
  {
    poolCreateInfo.poolSizeCount++;
    poolCreateInfo.pNext = &inlineCreateInfo;
  }

  poolCreateInfo.poolSizeCount += mutablePoolsizeBitmask.count();

  if(!mutablePoolsizeBitmask.empty())
  {
    mutableTypeLists.clear();
    mutableTypeLists.resize(poolCreateInfo.poolSizeCount);

    for(size_t i = 0; i < mutablePoolsizeBitmask.size(); i++)
    {
      mutableTypeLists[MutablePoolStart + i].pDescriptorTypes =
          mutableTypeArray.data() + mutableBitmaskArrayRange[i].first;
      mutableTypeLists[MutablePoolStart + i].descriptorTypeCount = mutableBitmaskArrayRange[i].second;
    }

    poolCreateInfo.pNext = &mutableCreateInfo;
    mutableCreateInfo.mutableDescriptorTypeListCount = (uint32_t)mutableTypeLists.size();
    mutableCreateInfo.pMutableDescriptorTypeLists = mutableTypeLists.data();
  }

  // create descriptor pool with enough space for our descriptors
  vkr = m_pDriver->vkCreateDescriptorPool(dev, &poolCreateInfo, NULL, &descpool);
  CheckVkResult(vkr);

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

  rdcarray<VkWriteDescriptorSet> descWrites;

  // copy the data across from the real descriptors into our adjusted bindings
  for(size_t i = 0; i < boundDescs; i++)
  {
    if(pipe.descSets[i].descSet == ResourceId())
      continue;

    const VulkanCreationInfo::PipelineLayout &pipelineLayoutInfo =
        creationInfo.m_PipelineLayout[pipe.descSets[i].pipeLayout];

    if(pipelineLayoutInfo.descSetLayouts[i] == ResourceId())
      continue;

    // as above we use the pipeline layout that was originally used to bind this descriptor set
    // and not the pipeline layout from the pipeline, in case the pipeline statically doesn't use
    // this set and so its descriptor set layout is garbage (doesn't match the actual bound
    // descriptor set)
    const DescSetLayout &origLayout =
        creationInfo.m_DescSetLayout[pipelineLayoutInfo.descSetLayouts[i]];

    WrappedVulkan::DescriptorSetInfo &setInfo =
        m_pDriver->m_DescriptorSetState[pipe.descSets[i].descSet];

    {
      // Only write bindings that actually exist in the current descriptor
      // set. If there are bindings that aren't set, assume the app knows
      // what it's doing and the remaining bindings are unused.
      for(size_t bind = 0; bind < setInfo.data.binds.size(); bind++)
      {
        const DescSetLayout::Binding &layoutBind = origLayout.bindings[bind];

        // skip empty bindings
        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          continue;

        uint32_t descriptorCount = layoutBind.descriptorCount;

        if(layoutBind.variableSize)
          descriptorCount = setInfo.data.variableDescriptorCount;

        if(descriptorCount == 0)
          continue;

        DescriptorSetSlot *slots = setInfo.data.binds[bind];

        // skip validity check for inline uniform block as the descriptor count means something
        // different
        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          allocInlineWrites.push_back(new VkWriteDescriptorSetInlineUniformBlock);
          VkWriteDescriptorSetInlineUniformBlock *inlineWrite = allocInlineWrites.back();
          inlineWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
          inlineWrite->pNext = NULL;
          inlineWrite->pData = setInfo.data.inlineBytes.data() + slots->offset;
          inlineWrite->dataSize = descriptorCount;

          VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
          write.pNext = inlineWrite;
          write.dstSet = descSets[i];
          write.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
          write.dstBinding = uint32_t(bind + newBindingsCount);
          write.descriptorCount = descriptorCount;

          descWrites.push_back(write);
          continue;
        }

        // skip single descriptors that are not valid
        if(!m_pDriver->NULLDescriptorsAllowed() && descriptorCount == 1 &&
           slots->resource == ResourceId() && slots->sampler == ResourceId())
        {
          // do nothing - don't increment bind so that the same write descriptor is used next time.
          continue;
        }

        VkDescriptorBufferInfo *writeScratch = new VkDescriptorBufferInfo[descriptorCount];
        allocBufWrites.push_back(writeScratch);

        CreateDescriptorWritesForSlotData(m_pDriver, descWrites, writeScratch, slots,
                                          descriptorCount, descSets[i],
                                          uint32_t(bind + newBindingsCount), layoutBind);
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

  GpaVkContextOpenInfo context = {Unwrap(m_pDriver->GetInstance()), Unwrap(m_pDriver->GetPhysDev()),
                                  Unwrap(m_pDriver->GetDev())};

  if(!m_pDriver->GetReplay()->IsRemoteProxy() && Vulkan_HardwareCounters())
  {
    GPUVendor vendor = m_pDriver->GetDriverInfo().Vendor();

    if(vendor == GPUVendor::AMD || vendor == GPUVendor::Samsung)
    {
      RDCLOG("AMD GPU detected - trying to initialise AMD counters");
      AMDCounters *counters = new AMDCounters();
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
#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
    else if(vendor == GPUVendor::nVidia)
    {
      RDCLOG("NVIDIA GPU detected - trying to initialise NVIDIA counters");
      NVVulkanCounters *countersNV = new NVVulkanCounters();
      bool initSuccess = false;
      if(countersNV && countersNV->Init(m_pDriver))
      {
        m_pNVCounters = countersNV;
        initSuccess = true;
      }
      else
      {
        delete countersNV;
      }
      RDCLOG("NVIDIA Vulkan counter initialisation: %s", initSuccess ? "SUCCEEDED" : "FAILED");
    }
#endif
    else
    {
      RDCLOG("%s GPU detected - no counters available", ToStr(vendor).c_str());
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

#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
  SAFE_DELETE(m_pNVCounters);
#endif
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
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32},
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
  driver->CheckVkResult(vkr);

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
        StencilMode::KEEP,
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

        // reuse float 'green' as srgb
        if(f == 0 && i == 0)
        {
          CREATE_OBJECT(texRemapInfo.renderPass, VK_FORMAT_R8G8B8A8_SRGB);

          CREATE_OBJECT(RemapPipeline[f][i][1], texRemapInfo);

          driver->vkDestroyRenderPass(driver->GetDev(), texRemapInfo.renderPass, NULL);
        }
      }
    }

    // make versions that only write to green, for doing two-pass stencil writes
    texRemapInfo.writeMask = texDisplayInfo.writeMask = 0x2;

    for(int f = 0; f < 3; f++)
    {
      // only create this for uint, it's normally only needed there
      int i = 1;

      texRemapInfo.fragment =
          shaderCache->GetBuiltinModule(BuiltinShader::TexRemap, BuiltinShaderBaseType(i));

      CREATE_OBJECT(texRemapInfo.renderPass, GetViewCastedFormat(formats[f], cast[i]));

      CREATE_OBJECT(RemapPipeline[f][i][1], texRemapInfo);

      driver->vkDestroyRenderPass(driver->GetDev(), texRemapInfo.renderPass, NULL);
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

    if(cmd == VK_NULL_HANDLE)
      return;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    driver->CheckVkResult(vkr);

    int index = 0;

    // we pick RGBA8 formats to be guaranteed they will be supported
    VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UINT,
                          VK_FORMAT_R8G8B8A8_SINT, VK_FORMAT_D16_UNORM};
    VkImageType types[] = {VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_2D};
    VkImageViewType viewtypes[] = {
        VK_IMAGE_VIEW_TYPE_1D_ARRAY,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VK_IMAGE_VIEW_TYPE_3D,
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

        // make the 2D image cube-compatible for non-depth
        if(type == 1)
        {
          imInfo.arrayLayers = 6;
          imInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        // some depth images might not be supported
        if(formats[fmt] == VK_FORMAT_D16_UNORM)
        {
          VkImageFormatProperties props = {};
          vkr = driver->vkGetPhysicalDeviceImageFormatProperties(
              driver->GetPhysDev(), imInfo.format, imInfo.imageType, imInfo.tiling, imInfo.usage,
              imInfo.flags, &props);

          if(vkr != VK_SUCCESS)
          {
            if(type == 1)
            {
              // create non-cube compatible
              imInfo.arrayLayers = 1;
              imInfo.flags = 0;

              DepthCubesSupported = false;
            }
            else
            {
              RDCLOG("Couldn't create image with format %s type %s and sample count %s",
                     ToStr(formats[fmt]).c_str(), ToStr(types[type]).c_str(),
                     ToStr(sampleCounts[type]).c_str());
              continue;
            }
          }
        }

        vkr = driver->vkCreateImage(driver->GetDev(), &imInfo, NULL, &DummyImages[fmt][type]);
        driver->CheckVkResult(vkr);

        NameVulkanObject(DummyImages[fmt][type],
                         "DummyImages[" + ToStr(fmt) + "][" + ToStr(type) + "]");

        MemoryAllocation alloc = driver->AllocateMemoryForResource(
            DummyImages[fmt][type], MemoryScope::ImmutableReplayDebug, MemoryType::GPULocal);

        if(alloc.mem == VK_NULL_HANDLE)
          return;

        vkr = driver->vkBindImageMemory(driver->GetDev(), DummyImages[fmt][type], alloc.mem,
                                        alloc.offs);
        driver->CheckVkResult(vkr);

        // don't add dummy writes/infos for depth, we just want the images and views
        if(formats[fmt] == VK_FORMAT_D16_UNORM)
          continue;

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

    RDCASSERT(index + 1 < (int)ARRAY_COUNT(DummyInfos));

    // align up for the dummy buffer
    {
      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,     NULL, 0, 16,
          VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
      };

      vkr = driver->vkCreateBuffer(driver->GetDev(), &bufInfo, NULL, &DummyBuffer);
      driver->CheckVkResult(vkr);

      MemoryAllocation alloc = driver->AllocateMemoryForResource(
          DummyBuffer, MemoryScope::ImmutableReplayDebug, MemoryType::GPULocal);

      if(alloc.mem == VK_NULL_HANDLE)
        return;

      vkr = driver->vkBindBufferMemory(driver->GetDev(), DummyBuffer, alloc.mem, alloc.offs);
      driver->CheckVkResult(vkr);
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

        // don't make cube views if cubes weren't supported for depth
        if(formats[fmt] == VK_FORMAT_D16_UNORM && cube && !DepthCubesSupported)
          continue;

        // don't create views when we failed to make the images
        if(DummyImages[fmt][imType] == VK_NULL_HANDLE)
          continue;

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
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1,
            },
        };

        if(formats[fmt] == VK_FORMAT_D16_UNORM)
          viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(cube)
          viewInfo.subresourceRange.layerCount = 6;

        vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL,
                                        &DummyImageViews[fmt][type]);
        driver->CheckVkResult(vkr);

        NameVulkanObject(DummyImageViews[fmt][type],
                         "DummyImageViews[" + ToStr(fmt) + "][" + ToStr(type) + "]");

        // the cubemap view we don't create an info for it, and the image is already transitioned
        if(cube)
          continue;

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

        if(formats[fmt] == VK_FORMAT_D16_UNORM)
          barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        DoPipelineBarrier(cmd, 1, &barrier);

        if(formats[fmt] == VK_FORMAT_D16_UNORM)
          continue;

        RDCASSERT((size_t)index < ARRAY_COUNT(DummyInfos), index);

        DummyInfos[index].imageView = Unwrap(DummyImageViews[fmt][type]);

        index++;
      }
    }

    // duplicate 2D dummy image into YUV
    DummyInfos[index].imageView = DummyInfos[1].imageView;
    DummyInfos[index + 1].imageView = DummyInfos[1].imageView;

    RDCASSERT(index + 1 < (int)ARRAY_COUNT(DummyInfos));

    if(DummyBuffer != VK_NULL_HANDLE)
    {
      VkFormat bufViewTypes[] = {
          VK_FORMAT_R32G32B32A32_SFLOAT,
          VK_FORMAT_R32G32B32A32_UINT,
          VK_FORMAT_R32G32B32A32_SINT,
      };
      for(size_t i = 0; i < ARRAY_COUNT(bufViewTypes); i++)
      {
        VkBufferViewCreateInfo viewInfo = {
            VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL, 0, DummyBuffer, bufViewTypes[i], 0, 16,
        };

        vkr = driver->vkCreateBufferView(driver->GetDev(), &viewInfo, NULL, &DummyBufferView[i]);
        driver->CheckVkResult(vkr);
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

  CREATE_OBJECT(m_PointSampler, VK_FILTER_NEAREST);

  CREATE_OBJECT(m_CheckerDescSetLayout,
                {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL}});

  CREATE_OBJECT(m_QuadDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  CREATE_OBJECT(m_TriSizeDescSetLayout,
                {
                    {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
                    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL},
                    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
                });

  CREATE_OBJECT(m_DepthCopyDescSetLayout, {
                                              {
                                                  0,
                                                  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  1,
                                                  VK_SHADER_STAGE_ALL,
                                                  &m_PointSampler,
                                              },
                                          });

  CREATE_OBJECT(m_CheckerPipeLayout, m_CheckerDescSetLayout, 0);
  CREATE_OBJECT(m_QuadResolvePipeLayout, m_QuadDescSetLayout, 0);
  CREATE_OBJECT(m_TriSizePipeLayout, m_TriSizeDescSetLayout, 0);
  CREATE_OBJECT(m_DepthCopyPipeLayout, m_DepthCopyDescSetLayout, 0);
  CREATE_OBJECT(m_QuadDescSet, descriptorPool, m_QuadDescSetLayout);
  CREATE_OBJECT(m_TriSizeDescSet, descriptorPool, m_TriSizeDescSetLayout);
  CREATE_OBJECT(m_CheckerDescSet, descriptorPool, m_CheckerDescSetLayout);
  CREATE_OBJECT(m_DepthCopyDescSet, descriptorPool, m_DepthCopyDescSetLayout);

  m_CheckerUBO.Create(driver, driver->GetDev(), 128, 10, 0);
  RDCCOMPILE_ASSERT(sizeof(CheckerboardUBOData) <= 128, "checkerboard UBO size");

  m_DummyMeshletSSBO.Create(driver, driver->GetDev(), sizeof(Vec4f) * 2, 1,
                            GPUBuffer::eGPUBufferSSBO);
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
      StencilMode::KEEP,
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

  uint32_t supportedColorSampleCounts = driver->GetDeviceProps().limits.framebufferColorSampleCounts;

  for(size_t i = 0; i < ARRAY_COUNT(m_CheckerF16Pipeline); i++)
  {
    VkSampleCountFlagBits samples = VkSampleCountFlagBits(1 << i);

    if((supportedColorSampleCounts & (uint32_t)samples) == 0)
      continue;

    VkRenderPass RGBA16MSRP = VK_NULL_HANDLE;

    CREATE_OBJECT(RGBA16MSRP, VK_FORMAT_R16G16B16A16_SFLOAT, samples);

    if(RGBA16MSRP != VK_NULL_HANDLE)
      samplesHandled |= (uint32_t)samples;
    else
      continue;

    // if we know this sample count is supported then create a pipeline
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

  uint32_t supportedDepthSampleCounts = driver->GetDeviceProps().limits.framebufferDepthSampleCounts;

  samplesHandled = 0;
  {
    ConciseGraphicsPipeline DepthCopyPipeInfo = {
        SRGBA8RP,
        m_DepthCopyPipeLayout,
        shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
        shaderCache->GetBuiltinModule(BuiltinShader::DepthCopyFS),
        {VK_DYNAMIC_STATE_VIEWPORT},
        VK_SAMPLE_COUNT_1_BIT,
        false,    // sampleRateShading
        true,     // depthEnable
        true,     // stencilEnable
        StencilMode::WRITE_ZERO,
        true,     // colourOutput
        false,    // blendEnable
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE,
        0x0,    // writeMask
    };

    VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dsRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkAttachmentDescription attDescs[] = {
        {
            0,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            colRef.layout,
            colRef.layout,
        },
        {
            0,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            dsRef.layout,
            dsRef.layout,
        },
    };

    VkSubpassDescription subp = {
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
        &subp,
        0,
        NULL,    // dependencies
    };

    RDCCOMPILE_ASSERT(ARRAY_COUNT(m_DepthCopyPipeline) == ARRAY_COUNT(m_DepthResolvePipeline),
                      "m_DepthCopyPipeline size must match m_DepthResolvePipeline");

    if(DepthCopyPipeInfo.fragment != VK_NULL_HANDLE)
    {
      for(size_t f = 0; f < ARRAY_COUNT(m_DepthCopyPipeline); ++f)
      {
        for(size_t i = 0; i < ARRAY_COUNT(m_DepthCopyPipeline[f]); ++i)
          m_DepthCopyPipeline[f][i] = VK_NULL_HANDLE;

        VkFormat fmt = (f == 0) ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
        VkImageFormatProperties props;
        if(driver->vkGetPhysicalDeviceImageFormatProperties(
               driver->GetPhysDev(), fmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &props) != VK_SUCCESS)
          continue;

        attDescs[1].format = fmt;
        for(size_t i = 0; i < ARRAY_COUNT(m_DepthCopyPipeline[f]); ++i)
        {
          VkSampleCountFlagBits samples = VkSampleCountFlagBits(1 << i);
          if((supportedDepthSampleCounts & (uint32_t)samples) == 0)
            continue;

          VkRenderPass depthMSRP = VK_NULL_HANDLE;
          attDescs[0].samples = samples;
          attDescs[1].samples = samples;
          VkResult vkr = driver->vkCreateRenderPass(driver->GetDev(), &rpinfo, NULL, &depthMSRP);
          if(vkr != VK_SUCCESS)
            RDCERR("Failed to create depth overlay resolve render pass: %s", ToStr(vkr).c_str());

          if(depthMSRP != VK_NULL_HANDLE)
            samplesHandled |= (uint32_t)samples;
          else
            continue;

          // if we know this sample count is supported then create a pipeline
          DepthCopyPipeInfo.renderPass = depthMSRP;
          DepthCopyPipeInfo.sampleCount = VkSampleCountFlagBits(1 << i);

          if(i == 0)
            DepthCopyPipeInfo.fragment = shaderCache->GetBuiltinModule(BuiltinShader::DepthCopyFS);
          else
            DepthCopyPipeInfo.fragment = shaderCache->GetBuiltinModule(BuiltinShader::DepthCopyMSFS);

          CREATE_OBJECT(m_DepthCopyPipeline[f][i], DepthCopyPipeInfo);

          driver->vkDestroyRenderPass(driver->GetDev(), depthMSRP, NULL);
        }
      }
    }
  }
  RDCASSERTEQUAL((uint32_t)driver->GetDeviceProps().limits.framebufferDepthSampleCounts,
                 samplesHandled);

  samplesHandled = 0;
  {
    // make patched shader
    VkShaderModule greenFSmod = VK_NULL_HANDLE;
    float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
    driver->GetDebugManager()->PatchFixedColShader(greenFSmod, green);

    CREATE_OBJECT(m_DepthResolvePipeLayout, VK_NULL_HANDLE, 0);

    ConciseGraphicsPipeline DepthResolvePipeInfo = {
        SRGBA8RP,
        m_DepthResolvePipeLayout,
        shaderCache->GetBuiltinModule(BuiltinShader::BlitVS),
        greenFSmod,
        {VK_DYNAMIC_STATE_VIEWPORT},
        VK_SAMPLE_COUNT_1_BIT,
        false,    // sampleRateShading
        false,    // depthEnable
        true,     // stencilEnable
        StencilMode::KEEP_TEST_EQUAL_ONE,
        true,     // colourOutput
        false,    // blendEnable
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE,
        0xf,    // writeMask
    };

    VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dsRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkAttachmentDescription attDescs[] = {
        {
            0,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            colRef.layout,
            colRef.layout,
        },
        {
            0,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            dsRef.layout,
            dsRef.layout,
        },
    };

    VkSubpassDescription subp = {
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
        &subp,
        0,
        NULL,    // dependencies
    };

    if(DepthResolvePipeInfo.fragment != VK_NULL_HANDLE)
    {
      for(size_t f = 0; f < ARRAY_COUNT(m_DepthResolvePipeline); ++f)
      {
        for(size_t i = 0; i < ARRAY_COUNT(m_DepthResolvePipeline[f]); ++i)
          m_DepthResolvePipeline[f][i] = VK_NULL_HANDLE;

        VkFormat fmt = (f == 0) ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
        VkImageFormatProperties props;
        if(driver->vkGetPhysicalDeviceImageFormatProperties(
               driver->GetPhysDev(), fmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &props) != VK_SUCCESS)
          continue;

        attDescs[1].format = fmt;
        for(size_t i = 0; i < ARRAY_COUNT(m_DepthResolvePipeline[f]); ++i)
        {
          VkSampleCountFlagBits samples = VkSampleCountFlagBits(1 << i);
          if((supportedDepthSampleCounts & (uint32_t)samples) == 0)
            continue;

          VkRenderPass rgba16MSRP = VK_NULL_HANDLE;
          attDescs[0].samples = samples;
          attDescs[1].samples = samples;
          VkResult vkr = driver->vkCreateRenderPass(driver->GetDev(), &rpinfo, NULL, &rgba16MSRP);
          if(vkr != VK_SUCCESS)
            RDCERR("Failed to create depth overlay resolve render pass: %s", ToStr(vkr).c_str());

          if(rgba16MSRP != VK_NULL_HANDLE)
            samplesHandled |= (uint32_t)samples;
          else
            continue;

          // if we know this sample count is supported then create a pipeline
          DepthResolvePipeInfo.renderPass = rgba16MSRP;
          DepthResolvePipeInfo.sampleCount = VkSampleCountFlagBits(1 << i);

          CREATE_OBJECT(m_DepthResolvePipeline[f][i], DepthResolvePipeInfo);

          driver->vkDestroyRenderPass(driver->GetDev(), rgba16MSRP, NULL);
        }
      }
    }
  }

  RDCASSERTEQUAL((uint32_t)driver->GetDeviceProps().limits.framebufferDepthSampleCounts,
                 samplesHandled);

  m_DefaultDepthStencilFormat = VK_FORMAT_UNDEFINED;
  {
    for(VkFormat fmt : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT})
    {
      VkImageFormatProperties imgprops = {};
      VkResult vkr = driver->vkGetPhysicalDeviceImageFormatProperties(
          driver->GetPhysDev(), fmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &imgprops);
      if(vkr == VK_SUCCESS)
      {
        m_DefaultDepthStencilFormat = fmt;
        break;
      }
    }
  }
  if(m_DefaultDepthStencilFormat == VK_FORMAT_UNDEFINED)
  {
    RDCERR("Overlay failed to find default depth stencil format");
  }

  VkDescriptorBufferInfo meshssbo = {};
  m_DummyMeshletSSBO.FillDescriptor(meshssbo);

  VkDescriptorBufferInfo checkerboard = {};
  m_CheckerUBO.FillDescriptor(checkerboard);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_TriSizeDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &meshssbo, NULL},
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

  driver->vkDestroyPipelineLayout(driver->GetDev(), m_DepthResolvePipeLayout, NULL);
  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), m_DepthCopyDescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), m_DepthCopyPipeLayout, NULL);

  for(size_t f = 0; f < ARRAY_COUNT(m_DepthResolvePipeline); ++f)
  {
    for(size_t i = 0; i < ARRAY_COUNT(m_DepthResolvePipeline[f]); ++i)
    {
      driver->vkDestroyPipeline(driver->GetDev(), m_DepthResolvePipeline[f][i], NULL);
      driver->vkDestroyPipeline(driver->GetDev(), m_DepthCopyPipeline[f][i], NULL);
    }
  }

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

  driver->vkDestroySampler(driver->GetDev(), m_PointSampler, NULL);
}

void VulkanReplay::MeshRendering::Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool)
{
  CREATE_OBJECT(
      DescSetLayout,
      {
          {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL},
      });

  CREATE_OBJECT(PipeLayout, DescSetLayout, 0);
  CREATE_OBJECT(DescSet, descriptorPool, DescSetLayout);

  UBO.Create(driver, driver->GetDev(), sizeof(MeshUBOData), 16, 0);
  MeshletSSBO.Create(driver, driver->GetDev(), sizeof(uint32_t) * (4 + MAX_NUM_MESHLETS), 16,
                     GPUBuffer::eGPUBufferSSBO);
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
      Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
      Vec4f(1.0f, 0.0f, 0.0f, 1.0f),
      Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
      Vec4f(0.0f, 1.0f, 0.0f, 1.0f),
      Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
      Vec4f(0.0f, 0.0f, 1.0f, 1.0f),

      // frustum vertices
      TLN,
      TRN,
      TRN,
      BRN,
      BRN,
      BLN,
      BLN,
      TLN,

      TLN,
      TLF,
      TRN,
      TRF,
      BLN,
      BLF,
      BRN,
      BRF,

      TLF,
      TRF,
      TRF,
      BRF,
      BRF,
      BLF,
      BLF,
      TLF,
  };

  // doesn't need to be ring'd as it's immutable
  AxisFrustumVB.Create(driver, driver->GetDev(), sizeof(axisFrustum), 1,
                       GPUBuffer::eGPUBufferVBuffer);

  Vec4f *axisData = (Vec4f *)AxisFrustumVB.Map();

  if(axisData)
    memcpy(axisData, axisFrustum, sizeof(axisFrustum));

  AxisFrustumVB.Unmap();

  VkDescriptorBufferInfo meshubo = {};
  VkDescriptorBufferInfo meshssbo = {};

  UBO.FillDescriptor(meshubo);
  MeshletSSBO.FillDescriptor(meshssbo);

  VkWriteDescriptorSet writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(DescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &meshubo, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(DescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, NULL, &meshssbo, NULL},
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
  MeshletSSBO.Destroy();
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
  driver->CheckVkResult(vkr);

  NameVulkanObject(Image, "PixelPick.Image");

  VkMemoryRequirements mrq = {0};
  driver->vkGetImageMemoryRequirements(driver->GetDev(), Image, &mrq);

  // allocate readback memory
  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      NULL,
      mrq.size,
      driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
  };

  vkr = driver->vkAllocateMemory(driver->GetDev(), &allocInfo, NULL, &ImageMem);
  driver->CheckVkResult(vkr);

  vkr = driver->vkBindImageMemory(driver->GetDev(), Image, ImageMem, 0);
  driver->CheckVkResult(vkr);

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
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          1,
          0,
          1,
      },
  };

  vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &ImageView);
  driver->CheckVkResult(vkr);

  NameVulkanObject(ImageView, "PixelPick.ImageView");

  // need to update image layout into valid state

  VkCommandBuffer cmd = driver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  driver->CheckVkResult(vkr);

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
  driver->CheckVkResult(vkr);

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
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
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
  vkr = driver->vkCreateDescriptorPool(driver->GetDev(), &descPoolInfo, NULL, &MSCopyDescPool);
  driver->CheckVkResult(vkr);

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
          // ShaderDebugBind::MathResult
          {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
           VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, NULL},
      });

  CREATE_OBJECT(PipeLayout, DescSetLayout, sizeof(Vec4f) * 6 + sizeof(uint32_t));

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
  driver->CheckVkResult(vkr);

  NameVulkanObject(Image, "ShaderDebugData.Image");

  VkMemoryRequirements mrq = {0};
  driver->vkGetImageMemoryRequirements(driver->GetDev(), Image, &mrq);

  // allocate readback memory
  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      NULL,
      mrq.size,
      driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
  };

  vkr = driver->vkAllocateMemory(driver->GetDev(), &allocInfo, NULL, &ImageMemory);
  driver->CheckVkResult(vkr);

  vkr = driver->vkBindImageMemory(driver->GetDev(), Image, ImageMemory, 0);
  driver->CheckVkResult(vkr);

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
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          1,
          0,
          1,
      },
  };

  vkr = driver->vkCreateImageView(driver->GetDev(), &viewInfo, NULL, &ImageView);
  driver->CheckVkResult(vkr);

  NameVulkanObject(ImageView, "ShaderDebugData.ImageView");

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
          VK_SUBPASS_EXTERNAL,
          0,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_TRANSFER_READ_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          0,
      },
      {
          0,
          VK_SUBPASS_EXTERNAL,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT,
          0,
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
  driver->CheckVkResult(vkr);

  MathResult.Create(driver, driver->GetDev(), sizeof(Vec4f) * 4, 1,
                    GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);

  // don't need to ring this, as we hard-sync for readback anyway
  ReadbackBuffer.Create(driver, driver->GetDev(), sizeof(Vec4f) * 4, 1,
                        GPUBuffer::eGPUBufferReadback);
  ConstantsBuffer.Create(driver, driver->GetDev(), 1024, 1, 0);
}

void ShaderDebugData::Destroy(WrappedVulkan *driver)
{
  ConstantsBuffer.Destroy();
  ReadbackBuffer.Destroy();

  for(size_t i = 0; i < ARRAY_COUNT(MathPipe); i++)
    driver->vkDestroyPipeline(driver->GetDev(), MathPipe[i], NULL);

  driver->vkDestroyDescriptorSetLayout(driver->GetDev(), DescSetLayout, NULL);
  driver->vkDestroyPipelineLayout(driver->GetDev(), PipeLayout, NULL);

  driver->vkDestroyImage(driver->GetDev(), Image, NULL);
  driver->vkFreeMemory(driver->GetDev(), ImageMemory, NULL);
  driver->vkDestroyImageView(driver->GetDev(), ImageView, NULL);
  driver->vkDestroyFramebuffer(driver->GetDev(), Framebuffer, NULL);
  driver->vkDestroyRenderPass(driver->GetDev(), RenderPass, NULL);

  // one module each for float, uint, sint.
  for(size_t i = 0; i < ARRAY_COUNT(Module); i++)
    driver->vkDestroyShaderModule(driver->GetDev(), Module[i], NULL);

  for(auto it = m_Pipelines.begin(); it != m_Pipelines.end(); it++)
    driver->vkDestroyPipeline(driver->GetDev(), it->second, NULL);
}
