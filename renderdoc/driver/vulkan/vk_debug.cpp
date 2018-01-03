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

#include "vk_debug.h"
#include <float.h>
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "data/glsl_shaders.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/debuguniforms.h"

const VkDeviceSize STAGE_BUFFER_BYTE_SIZE = 16 * 1024 * 1024ULL;

VulkanDebugManager::VulkanDebugManager(WrappedVulkan *driver, VkDevice dev)
{
  m_pDriver = driver;
  m_State = m_pDriver->GetState();

  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  driver->GetReplay()->PostDeviceInitCounters();

  m_ResourceManager = m_pDriver->GetResourceManager();

  m_Device = dev;

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Do some work that's needed both during capture and during replay

  VkResult vkr = VK_SUCCESS;

  // create linear sampler
  VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampInfo.minFilter = sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.maxLod = 128.0f;

  vkr = m_pDriver->vkCreateSampler(dev, &sampInfo, NULL, &m_LinearSampler);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkDescriptorPoolSize descPoolTypes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 320},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 128},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
  };

  VkDescriptorPoolCreateInfo descpoolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      10 + ARRAY_COUNT(m_TexDisplayDescSet),
      ARRAY_COUNT(descPoolTypes),
      &descPoolTypes[0],
  };

  // during capture we are only creating MS <-> Array copy pipelines, so our pool is significantly
  // smaller in size.
  if(IsCaptureMode(m_State))
  {
    // 1 set only
    descpoolInfo.maxSets = 1;

    // only need VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER and VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    descpoolInfo.poolSizeCount = 2;
    descPoolTypes[0].descriptorCount = 2;
    descPoolTypes[1].descriptorCount = 1;

    RDCASSERTMSG("Expected combined image/sampler as first pool descriptor type",
                 descPoolTypes[0].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 descPoolTypes[0].type);
    RDCASSERTMSG("Expected storage image as first pool descriptor type",
                 descPoolTypes[1].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descPoolTypes[1].type);
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

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  // needed in both replay and capture, create MS <-> array pipelines
  {
    {
      VkDescriptorSetLayoutBinding layoutBinding[] = {
          {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
          {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
          {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
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

    //////////////////////////////////////////////////////////////////
    // Color MS to Array copy (via compute)

    compPipeInfo.stage.module = shaderCache->GetBuiltinModule(BuiltinShader::MS2ArrayCS);
    compPipeInfo.layout = m_ArrayMSPipeLayout;

    if(compPipeInfo.stage.module != VK_NULL_HANDLE)
    {
      vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                &m_MS2ArrayPipe);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    compPipeInfo.stage.module = shaderCache->GetBuiltinModule(BuiltinShader::Array2MSCS);
    compPipeInfo.layout = m_ArrayMSPipeLayout;

    if(compPipeInfo.stage.module != VK_NULL_HANDLE)
    {
      vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                                &m_Array2MSPipe);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    //////////////////////////////////////////////////////////////////
    // Depth MS to Array copy (via graphics)

    descSetAllocInfo.pSetLayouts = &m_ArrayMSDescSetLayout;
    vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_ArrayMSDescSet);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    stages[0].module = shaderCache->GetBuiltinModule(BuiltinShader::BlitVS);
    stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::DepthMS2ArrayFS);

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
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &attDesc, 1, &sub,
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
      stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::DepthMS2ArrayFS);

      // initialise to NULL
      m_DepthMS2ArrayPipe[f] = VK_NULL_HANDLE;
      for(size_t s = 0; s < ARRAY_COUNT(sampleCounts); s++)
        m_DepthArray2MSPipe[f][s] = VK_NULL_HANDLE;

      // if the format isn't supported at all, bail out and don't try to create anything
      if(!(m_pDriver->GetFormatProperties(attDesc.format).optimalTilingFeatures &
           VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
      {
        RDCDEBUG("Depth copies MSAA -> Array not supported for format %s",
                 ToStr(attDesc.format).c_str());
        continue;
      }

      VkRenderPass rp;

      vkr = m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &rp);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      pipeInfo.renderPass = rp;

      vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                                 &m_DepthMS2ArrayPipe[f]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->vkDestroyRenderPass(dev, rp, NULL);

      stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::DepthArray2MSFS);
      msaa.sampleShadingEnable = true;
      msaa.minSampleShading = 1.0f;

      for(size_t s = 0; s < ARRAY_COUNT(sampleCounts); s++)
      {
        attDesc.samples = sampleCounts[s];
        msaa.rasterizationSamples = sampleCounts[s];

        // if this sample count isn't supported, don't create it
        if(!(m_pDriver->GetDeviceProps().limits.framebufferDepthSampleCounts &
             (uint32_t)attDesc.samples))
        {
          RDCDEBUG("Depth copies Array -> MSAA not supported for sample count %u on format %s",
                   attDesc.samples, ToStr(attDesc.format).c_str());
          continue;
        }

        vkr = m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &rp);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        pipeInfo.renderPass = rp;

        vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                                   &m_DepthArray2MSPipe[f][s]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        m_pDriver->vkDestroyRenderPass(dev, rp, NULL);
      }
    }

    // restore pipeline state to normal
    cb.attachmentCount = 1;

    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    msaa.sampleShadingEnable = false;
    msaa.minSampleShading = 0.0f;

    pipeInfo.renderPass = RGBA8sRGBRP;
    dyn.dynamicStateCount = ARRAY_COUNT(dynstates);
    dyn.pDynamicStates = dynstates;
    pipeInfo.pDepthStencilState = &ds;
  }

  // if we're capturing, bail out after this point as we have created everything we need.
  if(IsCaptureMode(m_State))
    return;

  // create point sampler
  sampInfo.minFilter = VK_FILTER_NEAREST;
  sampInfo.magFilter = VK_FILTER_NEAREST;

  vkr = m_pDriver->vkCreateSampler(dev, &sampInfo, NULL, &m_PointSampler);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  {
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
    };

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
    VkDescriptorSetLayoutBinding layoutBinding[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
    };

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
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
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
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
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
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, NULL},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
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
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
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
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
        {19, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
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

  attState.blendEnable = false;

  pipeInfo.layout = m_CheckerboardPipeLayout;
  pipeInfo.renderPass = RGBA8sRGBRP;

  stages[0].module = shaderCache->GetBuiltinModule(BuiltinShader::BlitVS);
  stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::CheckerboardFS);

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

  stages[0].module = shaderCache->GetBuiltinModule(BuiltinShader::BlitVS);
  stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::TexDisplayFS);

  pipeInfo.layout = m_TexDisplayPipeLayout;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeInfo.renderPass = RGBA32RP;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayF32Pipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeInfo.renderPass = RGBA16RP;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayF16Pipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeInfo.renderPass = RGBA8sRGBRP;

  attState.blendEnable = true;
  attState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  attState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TexDisplayBlendPipeline);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  stages[0].module = shaderCache->GetBuiltinModule(BuiltinShader::BlitVS);
  stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::OutlineFS);

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

  stages[0].module = shaderCache->GetBuiltinModule(BuiltinShader::BlitVS);
  stages[1].module = shaderCache->GetBuiltinModule(BuiltinShader::QuadResolveFS);

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

  shaderCache->SetCaching(true);

  vector<string> sources;

  SPIRVCompilationSettings compileSettings;
  compileSettings.lang = SPIRVSourceLanguage::VulkanGLSL;

  for(size_t t = eTexType_1D; t < eTexType_Max; t++)
  {
    for(size_t f = 0; f < 3; f++)
    {
      VkShaderModule minmaxtile = VK_NULL_HANDLE;
      VkShaderModule minmaxresult = VK_NULL_HANDLE;
      VkShaderModule histogram = VK_NULL_HANDLE;
      std::string err;
      SPIRVBlob blob = NULL;
      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, 0, NULL,
      };

      string defines = "";
      if(m_pDriver->GetDriverVersion().TexelFetchBrokenDriver())
        defines += "#define NO_TEXEL_FETCH\n";
      defines += string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
      defines += string("#define UINT_TEX ") + (f == 1 ? "1" : "0") + "\n";
      defines += string("#define SINT_TEX ") + (f == 2 ? "1" : "0") + "\n";

      GenerateGLSLShader(sources, eShaderVulkan, defines, GetEmbeddedResource(glsl_histogram_comp),
                         430);

      compileSettings.stage = SPIRVShaderStage::Compute;
      err = shaderCache->GetSPIRVBlob(compileSettings, sources, blob);
      RDCASSERT(err.empty() && blob);

      modinfo.codeSize = blob->size() * sizeof(uint32_t);
      modinfo.pCode = blob->data();

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &histogram);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GenerateGLSLShader(sources, eShaderVulkan, defines, GetEmbeddedResource(glsl_minmaxtile_comp),
                         430);

      err = shaderCache->GetSPIRVBlob(compileSettings, sources, blob);
      RDCASSERT(err.empty() && blob);

      modinfo.codeSize = blob->size() * sizeof(uint32_t);
      modinfo.pCode = blob->data();

      vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &minmaxtile);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(t == 1)
      {
        GenerateGLSLShader(sources, eShaderVulkan, defines,
                           GetEmbeddedResource(glsl_minmaxresult_comp), 430);

        err = shaderCache->GetSPIRVBlob(compileSettings, sources, blob);
        RDCASSERT(err.empty() && blob);

        modinfo.codeSize = blob->size() * sizeof(uint32_t);
        modinfo.pCode = blob->data();

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

  shaderCache->SetCaching(false);

  {
    compPipeInfo.stage.module = shaderCache->GetBuiltinModule(BuiltinShader::MeshCS);
    compPipeInfo.layout = m_MeshPickLayout;

    vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compPipeInfo, NULL,
                                              &m_MeshPickPipeline);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  m_pDriver->vkDestroyRenderPass(dev, RGBA16RP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA32RP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8sRGBRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8MSRP, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(RGBA16MSRP); i++)
    m_pDriver->vkDestroyRenderPass(dev, RGBA16MSRP[i], NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8LinearRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, BGRA8sRGBRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, BGRA8LinearRP, NULL);

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

  ClearPostVSCache();

  // since we don't have properly registered resources, releasing our descriptor
  // pool here won't remove the descriptor sets, so we need to free our own
  // tracking data (not the API objects) for descriptor sets.

  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(uint32_t i = 0; i < MeshDisplayPipelines::ePipe_Count; i++)
      m_pDriver->vkDestroyPipeline(dev, it->second.pipes[i], NULL);

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
  m_pDriver->vkDestroyPipeline(dev, m_TexDisplayF16Pipeline, NULL);
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
        m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, it->first);

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
        m_pDriver->GetShaderCache()->MakeComputePipelineInfo(pipeCreateInfo, it->first);

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
       m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::BlitVS), "main", NULL},
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

// TODO: Point meshes don't pick correctly
uint32_t VulkanDebugManager::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x,
                                        uint32_t y, uint32_t w, uint32_t h)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkLayerDispatchTable *vt = ObjDisp(dev);

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

  MeshPickUBOData *ubo = (MeshPickUBOData *)m_MeshPickUBO.Map();

  ubo->rayPos = rayPos;
  ubo->rayDir = rayDir;
  ubo->use_indices = cfg.position.indexByteStride ? 1U : 0U;
  ubo->numVerts = cfg.position.numIndices;
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

  bytebuf idxs;

  if(cfg.position.indexByteStride && cfg.position.indexResourceId != ResourceId())
    GetBufferData(cfg.position.indexResourceId, cfg.position.indexByteOffset, 0, idxs);

  // We copy into our own buffers to promote to the target type (uint32) that the
  // shader expects. Most IBs will be 16-bit indices, most VBs will not be float4.

  if(!idxs.empty())
  {
    // resize up on demand
    if(m_MeshPickIBSize < cfg.position.numIndices * sizeof(uint32_t))
    {
      if(m_MeshPickIBSize > 0)
      {
        m_MeshPickIB.Destroy();
        m_MeshPickIBUpload.Destroy();
      }

      m_MeshPickIBSize = cfg.position.numIndices * sizeof(uint32_t);

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
    if(cfg.position.indexByteStride == 2)
    {
      size_t bufsize = idxs.size() / 2;

      for(uint32_t i = 0; i < bufsize && i < cfg.position.numIndices; i++)
        outidxs[i] = idxs16[i];
    }
    else
    {
      size_t bufsize = idxs.size() / 4;

      memcpy(outidxs, idxs32, RDCMIN(bufsize, cfg.position.numIndices * sizeof(uint32_t)));
    }

    m_MeshPickIBUpload.Unmap();
  }

  if(m_MeshPickVBSize < cfg.position.numIndices * sizeof(FloatVector))
  {
    if(m_MeshPickVBSize > 0)
    {
      m_MeshPickVB.Destroy();
      m_MeshPickVBUpload.Destroy();
    }

    m_MeshPickVBSize = cfg.position.numIndices * sizeof(FloatVector);

    m_MeshPickVB.Create(m_pDriver, dev, m_MeshPickVBSize, 1,
                        GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO);
    m_MeshPickVBUpload.Create(m_pDriver, dev, m_MeshPickVBSize, 1, 0);
  }

  // unpack and linearise the data
  {
    bytebuf oldData;
    GetBufferData(cfg.position.vertexResourceId, cfg.position.vertexByteOffset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

    FloatVector *vbData = (FloatVector *)m_MeshPickVBUpload.Map();

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numIndices; i++)
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

  uint32_t workgroupx = uint32_t(cfg.position.numIndices / 128 + 1);
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

void VulkanDebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret)
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
