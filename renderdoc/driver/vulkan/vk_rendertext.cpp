/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "vk_rendertext.h"
#include "3rdparty/stb/stb_truetype.h"
#include "maths/matrix.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

VulkanTextRenderer::VulkanTextRenderer(WrappedVulkan *driver)
{
  m_pDriver = driver;
  m_Device = driver->GetDev();

  VulkanResourceManager *rm = driver->GetResourceManager();

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VulkanShaderCache *shaderCache = driver->GetShaderCache();

  // create linear sampler
  VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampInfo.minFilter = sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.maxLod = 128.0f;

  vkr = m_pDriver->vkCreateSampler(dev, &sampInfo, NULL, &m_LinearSampler);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_LinearSampler));

  // just need enough for text rendering
  VkDescriptorPoolSize captureDescPoolTypes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
  };

  VkDescriptorPoolCreateInfo descpoolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      2,
      ARRAY_COUNT(captureDescPoolTypes),
      &captureDescPoolTypes[0],
  };

  // create descriptor pool
  vkr = m_pDriver->vkCreateDescriptorPool(dev, &descpoolInfo, NULL, &m_DescriptorPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_DescriptorPool));

  // declare some common creation info structs
  VkPipelineLayoutCreateInfo pipeLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipeLayoutInfo.setLayoutCount = 1;

  VkDescriptorSetAllocateInfo descSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                  NULL, m_DescriptorPool, 1, NULL};

  // compatible render passes for creating pipelines.
  VkRenderPass RGBA8sRGBRP = VK_NULL_HANDLE;
  VkRenderPass RGBA8LinearRP = VK_NULL_HANDLE;
  VkRenderPass BGRA8sRGBRP = VK_NULL_HANDLE;
  VkRenderPass BGRA8LinearRP = VK_NULL_HANDLE;

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

    VkSubpassDescription sub = {0};

    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &attRef;

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &attDesc, 1, &sub,
    };

    attDesc.format = VK_FORMAT_R8G8B8A8_SRGB;
    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA8sRGBRP);
    rm->SetInternalResource(GetResID(RGBA8sRGBRP));

    attDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &RGBA8LinearRP);
    rm->SetInternalResource(GetResID(RGBA8LinearRP));

    attDesc.format = VK_FORMAT_B8G8R8A8_SRGB;
    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &BGRA8sRGBRP);
    rm->SetInternalResource(GetResID(BGRA8sRGBRP));

    attDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
    m_pDriver->vkCreateRenderPass(dev, &rpinfo, NULL, &BGRA8LinearRP);
    rm->SetInternalResource(GetResID(BGRA8LinearRP));
  }

  // declare the pipeline creation info and all of its sub-structures
  // these are modified as appropriate for each pipeline we create
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
       shaderCache->GetBuiltinModule(BuiltinShader::TextVS), "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
       shaderCache->GetBuiltinModule(BuiltinShader::TextFS), "main", NULL},
  };

  VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
  };

  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkRect2D scissor = {{0, 0}, {16384, 16384}};

  VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, &scissor};

  VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
  };
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
  };
  msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState attState = {
      true,
      VK_BLEND_FACTOR_SRC_ALPHA,
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
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
      NULL,
      &cb,
      &dyn,
      VK_NULL_HANDLE,
      VK_NULL_HANDLE,
      0,                 // sub pass
      VK_NULL_HANDLE,    // base pipeline handle
      -1,                // base pipeline index
  };

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkDescriptorSetLayoutBinding layoutBinding[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
      {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL},
      {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL},
      {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL},
  };

  VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      NULL,
      0,
      ARRAY_COUNT(layoutBinding),
      &layoutBinding[0],
  };

  vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL, &m_TextDescSetLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextDescSetLayout));

  pipeLayoutInfo.pSetLayouts = &m_TextDescSetLayout;

  vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &m_TextPipeLayout);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextPipeLayout));

  descSetAllocInfo.pSetLayouts = &m_TextDescSetLayout;
  vkr = m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_TextDescSet);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextDescSet));

  // make the ring conservatively large to handle many lines of text * several frames
  m_TextGeneralUBO.Create(driver, dev, 128, 100, 0);
  RDCCOMPILE_ASSERT(sizeof(FontUBOData) <= 128, "font uniforms size");

  rm->SetInternalResource(GetResID(m_TextGeneralUBO.buf));
  rm->SetInternalResource(GetResID(m_TextGeneralUBO.mem));

  // we only use a subset of the [MAX_SINGLE_LINE_LENGTH] array needed for each line, so this ring
  // can be smaller
  m_TextStringUBO.Create(driver, dev, 4096, 20, 0);
  RDCCOMPILE_ASSERT(sizeof(StringUBOData) <= 4096, "font uniforms size");

  rm->SetInternalResource(GetResID(m_TextStringUBO.buf));
  rm->SetInternalResource(GetResID(m_TextStringUBO.mem));

  pipeInfo.layout = m_TextPipeLayout;

  pipeInfo.renderPass = RGBA8sRGBRP;
  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TextPipeline[0]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextPipeline[0]));

  pipeInfo.renderPass = RGBA8LinearRP;
  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TextPipeline[1]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextPipeline[1]));

  pipeInfo.renderPass = BGRA8sRGBRP;
  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TextPipeline[2]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextPipeline[2]));

  pipeInfo.renderPass = BGRA8LinearRP;
  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                             &m_TextPipeline[3]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  rm->SetInternalResource(GetResID(m_TextPipeline[3]));

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
    };

    std::string font = GetEmbeddedResource(sourcecodepro_ttf);
    byte *ttfdata = (byte *)font.c_str();

    const int firstChar = FONT_FIRST_CHAR;
    const int lastChar = FONT_LAST_CHAR;
    const int numChars = lastChar - firstChar + 1;

    RDCCOMPILE_ASSERT(FONT_FIRST_CHAR == int(' '), "Font defines are messed up");

    byte *buf = new byte[width * height];

    const float pixelHeight = 20.0f;

    stbtt_bakedchar chardata[numChars];
    stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars, chardata);

    m_FontCharSize = pixelHeight;
#if ENABLED(RDOC_ANDROID)
    m_FontCharSize *= 2.0f;
#endif

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

      rm->SetInternalResource(GetResID(m_TextAtlas));

      VkMemoryRequirements mrq = {0};
      m_pDriver->vkGetImageMemoryRequirements(dev, m_TextAtlas, &mrq);

      // allocate readback memory
      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &m_TextAtlasMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      rm->SetInternalResource(GetResID(m_TextAtlasMem));

      vkr = m_pDriver->vkBindImageMemory(dev, m_TextAtlas, m_TextAtlasMem, 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageViewCreateInfo viewInfo = {
          VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          NULL,
          0,
          m_TextAtlas,
          VK_IMAGE_VIEW_TYPE_2D,
          imInfo.format,
          {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      };

      vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &m_TextAtlasView);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      rm->SetInternalResource(GetResID(m_TextAtlasView));

      // create temporary memory and buffer to upload atlas
      // doesn't need to be ring'd, as it's static
      m_TextAtlasUpload.Create(driver, dev, 32768, 1, 0);
      RDCCOMPILE_ASSERT(width * height <= 32768, "font uniform size");

      rm->SetInternalResource(GetResID(m_TextAtlasUpload.buf));
      rm->SetInternalResource(GetResID(m_TextAtlasUpload.mem));

      byte *pData = (byte *)m_TextAtlasUpload.Map();
      RDCASSERT(pData);

      memcpy(pData, buf, width * height);

      m_TextAtlasUpload.Unmap();
    }

    // doesn't need to be ring'd, as it's static
    m_TextGlyphUBO.Create(driver, dev, 4096, 1, 0);
    RDCCOMPILE_ASSERT(sizeof(Vec4f) * 2 * (numChars + 1) < 4096, "font uniform size");

    rm->SetInternalResource(GetResID(m_TextGlyphUBO.buf));
    rm->SetInternalResource(GetResID(m_TextGlyphUBO.mem));

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
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_TextAtlas),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

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
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_TextAtlas),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    // ensure atlas is filled before reading in shader
    DoPipelineBarrier(textAtlasUploadCmd, 1, &copydonebarrier);

    ObjDisp(textAtlasUploadCmd)->EndCommandBuffer(Unwrap(textAtlasUploadCmd));
  }

  VkDescriptorBufferInfo bufInfo[3];
  RDCEraseEl(bufInfo);

  m_TextGeneralUBO.FillDescriptor(bufInfo[0]);
  m_TextGlyphUBO.FillDescriptor(bufInfo[1]);
  m_TextStringUBO.FillDescriptor(bufInfo[2]);

  VkDescriptorImageInfo atlasImInfo;
  atlasImInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(textSetWrites), textSetWrites, 0, NULL);

  m_pDriver->vkDestroyRenderPass(dev, RGBA8sRGBRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, RGBA8LinearRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, BGRA8sRGBRP, NULL);
  m_pDriver->vkDestroyRenderPass(dev, BGRA8LinearRP, NULL);
}

VulkanTextRenderer::~VulkanTextRenderer()
{
  VkDevice dev = m_Device;

  m_pDriver->vkDestroyDescriptorPool(dev, m_DescriptorPool, NULL);

  m_pDriver->vkDestroySampler(dev, m_LinearSampler, NULL);

  m_pDriver->vkDestroyDescriptorSetLayout(dev, m_TextDescSetLayout, NULL);
  m_pDriver->vkDestroyPipelineLayout(dev, m_TextPipeLayout, NULL);
  for(size_t i = 0; i < ARRAY_COUNT(m_TextPipeline); i++)
    m_pDriver->vkDestroyPipeline(dev, m_TextPipeline[i], NULL);

  m_pDriver->vkDestroyImageView(dev, m_TextAtlasView, NULL);
  m_pDriver->vkDestroyImage(dev, m_TextAtlas, NULL);
  m_pDriver->vkFreeMemory(dev, m_TextAtlasMem, NULL);

  m_TextGeneralUBO.Destroy();
  m_TextGlyphUBO.Destroy();
  m_TextStringUBO.Destroy();
  m_TextAtlasUpload.Destroy();
}

void VulkanTextRenderer::BeginText(const TextPrintState &textstate)
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

void VulkanTextRenderer::RenderText(const TextPrintState &textstate, float x, float y,
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

void VulkanTextRenderer::RenderTextInternal(const TextPrintState &textstate, float x, float y,
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

void VulkanTextRenderer::EndText(const TextPrintState &textstate)
{
  ObjDisp(textstate.cmd)->CmdEndRenderPass(Unwrap(textstate.cmd));
}