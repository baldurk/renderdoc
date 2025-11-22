/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2025 Baldur Karlsson
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

#include "vk_test.h"

RD_TEST(VK_Pixel_History, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests pixel history";

  std::string common = R"EOSHADER(

#version 460 core

)EOSHADER";

  const std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;

layout(location = 0) INTERP out COL_TYPE outCol;

void main()
{
	gl_Position = vec4(Position.xyz, 1);
	outCol = ProcessColor(Color);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) INTERP in COL_TYPE inCol;

#if COLOR == 1
layout(location = 0, index = 0) out COL_TYPE Color;
#endif

void main()
{
  if (gl_FragCoord.x < DISCARD_X+1 && gl_FragCoord.x > DISCARD_X)
    discard;
  if (inCol.x > 10000 && inCol.y > 10000 && inCol.z > 10000)
    discard;
#if COLOR == 1
	Color = inCol + COL_TYPE(0, 0, 0, ProcessColor(vec4(ALPHA_ADD)).x);
#endif
}

)EOSHADER";

  std::string mspixel = R"EOSHADER(

#if COLOR == 1
layout(location = 0, index = 0) out COL_TYPE Color;
#endif

void main()
{
#if COLOR == 1
  vec4 col;
  if (gl_SampleID == 0)
    col = vec4(1, 0, 0, 1+ALPHA_ADD);
  else if (gl_SampleID == 1)
    col = vec4(0, 0, 1, 1+ALPHA_ADD);
  else if (gl_SampleID == 2)
    col = vec4(0, 1, 1, 1+ALPHA_ADD);
  else if (gl_SampleID == 3)
    col = vec4(1, 1, 1, 1+ALPHA_ADD);
  Color = ProcessColor(col);
#endif
}

)EOSHADER";

  std::string comp = R"EOSHADER(

layout(set = 0, binding = 0) writeonly uniform IMAGE_TYPE Output;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
  vec4 data = vec4(3, 3, 3, 9);

  for(int x=0; x < 10; x++)
    for(int y=0; y < 10; y++)
      imageStore(Output, ivec3(STORE_X+x, STORE_Y+y, STORE_Z) STORE_SAMPLE, ProcessColor(data));
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    features.depthBounds = true;
    features.geometryShader = true;
    features.sampleRateShading = true;
    features.shaderStorageImageMultisample = true;
    features.shaderStorageImageReadWithoutFormat = true;
    features.shaderStorageImageWriteWithoutFormat = true;

    devExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_D32_SFLOAT_S8_UINT, &props);
    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
    {
      Avail = "D32S8 not supported";
      return;
    }
  }

  struct VKTestBatch
  {
    std::string name;

    bool secondary = false;
    bool uint = false;

    AllocatedImage colImg;
    AllocatedImage depthImg;

    VkFormat depthFormat;

    VkDescriptorSet compset;
    VkPipelineLayout compLayout;
    VkPipeline comppipe;

    VkFramebuffer fb;
    VkRenderPass rp;

    VkExtent2D extent;

    VkPipeline depthWritePipe;
    VkPipeline fixedScissorPassPipe;
    VkPipeline fixedScissorFailPipe;
    VkPipeline dynamicStencilRefPipe;
    VkPipeline dynamicStencilMaskPipe;
    VkPipeline depthEqualPipe;
    VkPipeline depthPipe;
    VkPipeline stencilWritePipe;
    VkPipeline backgroundPipe;
    VkPipeline noFsPipe;
    VkPipeline basicPipe;
    VkPipeline colorMaskPipe;
    VkPipeline cullFrontPipe;
    VkPipeline depthBoundsPipe1;
    VkPipeline depthBoundsPipe2;
    VkPipeline sampleColourPipe;
  };

  std::vector<VKTestBatch> batches;

  void BuildTestBatch(const std::string &name, VkSampleCountFlagBits sampleCount,
                      VkFormat colourFormat, VkFormat depthStencilFormat, int targetMip = -1,
                      int targetSlice = -1, int targetDepthSlice = -1)
  {
    batches.push_back({});

    VKTestBatch &batch = batches.back();
    batch.name = name;
    batch.extent = mainWindow->scissor.extent;

    uint32_t arraySize = targetSlice >= 0 ? 5 : 1;
    uint32_t mips = targetMip >= 0 ? 4 : 1;
    uint32_t depth = targetDepthSlice >= 0 ? 14 : 0;

    uint32_t mip = (uint32_t)std::max(0, targetMip);
    uint32_t slice = (uint32_t)std::max(0, targetSlice);
    if(depth > 1)
      slice = (uint32_t)targetDepthSlice;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    std::string defines;
    defines +=
        "#define COLOR " + std::string(colourFormat == VK_FORMAT_UNDEFINED ? "0" : "1") + "\n";

    std::string imgprefix;
    if(colourFormat == VK_FORMAT_R8G8B8A8_UINT || colourFormat == VK_FORMAT_R16G16B16A16_UINT ||
       colourFormat == VK_FORMAT_R32G32B32A32_UINT)
    {
      batch.uint = true;
      imgprefix = "u";
      defines += R"(

#define COL_TYPE uvec4
#define ALPHA_ADD 3
#define INTERP flat

uvec4 ProcessColor(vec4 col)
{
  uvec4 ret = uvec4(16*col);

  if(col.x < 0.0f) ret.x = 0xfffffff0;
  if(col.y < 0.0f) ret.y = 0xfffffff0;
  if(col.z < 0.0f) ret.z = 0xfffffff0;


  return ret;
}

)";
    }
    else
    {
      defines += R"(

#define COL_TYPE vec4
#define ALPHA_ADD 1.75
#define INTERP 

vec4 ProcessColor(vec4 col)
{
  vec4 ret = col;

  // this obviously won't overflow F32 but it will overflow anything 16-bit and under
  if(col.x < 0.0f) ret.x = 100000.0f;
  if(col.y < 0.0f) ret.y = 100000.0f;
  if(col.z < 0.0f) ret.z = 100000.0f;

  return ret;
}

)";
    }

    defines += "#define DISCARD_X " + std::to_string(150 >> mip) + "\n";
    defines += "#define IMAGE_TYPE " +
               std::string(depth > 1                              ? imgprefix + "image3D"
                           : sampleCount != VK_SAMPLE_COUNT_1_BIT ? imgprefix + "image2DMSArray"
                                                                  : imgprefix + "image2DArray") +
               "\n";
    defines += "#define STORE_X " + std::to_string(220 >> mip) + "\n";
    defines += "#define STORE_Y " + std::to_string(80 >> mip) + "\n";
    defines += "#define STORE_Z " + std::to_string(slice) + "\n";
    defines += "#define STORE_SAMPLE " +
               std::string(sampleCount != VK_SAMPLE_COUNT_1_BIT ? ", 0" : "") + "\n";
    defines += "\n\n";

    VkPipelineShaderStageCreateInfo vertexShader =
        CompileShaderModule(common + defines + vertex, ShaderLang::glsl, ShaderStage::vert, "main");
    VkPipelineShaderStageCreateInfo fragmentShader =
        CompileShaderModule(common + defines + pixel, ShaderLang::glsl, ShaderStage::frag, "main");

    vkh::RenderPassCreator renderPassCreateInfo;

    std::vector<VkAttachmentReference> cols;
    std::vector<VkImageView> atts;
    VkImageView colView = VK_NULL_HANDLE;

    if(colourFormat != VK_FORMAT_UNDEFINED)
    {
      batch.colImg = AllocatedImage(
          this,
          vkh::ImageCreateInfo(batch.extent.width, batch.extent.height, depth, colourFormat,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                               mips, arraySize, sampleCount,
                               depth > 1 ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT : 0),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
      setName(batch.colImg.image, batch.name + " colImg");

      colView = createImageView(vkh::ImageViewCreateInfo(
          batch.colImg.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, colourFormat, {},
          vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, slice, 1)));
      setName(colView, batch.name + " colView");

      VkImageView storeView = createImageView(vkh::ImageViewCreateInfo(
          batch.colImg.image, depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
          colourFormat, {},
          vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0,
                                     VK_REMAINING_ARRAY_LAYERS)));
      setName(storeView, batch.name + " storeView");

      atts.push_back(colView);

      renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
          colourFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
          VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, sampleCount));

      cols = {VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})};

      VkDescriptorSetLayout compsetlayout =
          createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
              {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
          }));
      batch.compLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({compsetlayout}));

      batch.comppipe = createComputePipeline(vkh::ComputePipelineCreateInfo(
          batch.compLayout, CompileShaderModule(common + defines + comp, ShaderLang::glsl,
                                                ShaderStage::comp, "main")));

      batch.compset = allocateDescriptorSet(compsetlayout);

      vkh::updateDescriptorSets(
          device,
          {
              vkh::WriteDescriptorSet(
                  batch.compset, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                  {vkh::DescriptorImageInfo(storeView, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
          });
    }

    batch.depthFormat = depthStencilFormat;
    VkImageView depthView = VK_NULL_HANDLE;

    if(depthStencilFormat != VK_FORMAT_UNDEFINED)
    {
      // can't create 2D views of 3D with depth images, so we just use a normal 2D array image
      if(depth > 1)
      {
        arraySize = depth;
        depth = 0;
      }

      batch.depthImg = AllocatedImage(
          this,
          vkh::ImageCreateInfo(batch.extent.width, batch.extent.height, 0, depthStencilFormat,
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, mips, arraySize,
                               sampleCount),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
      setName(batch.depthImg.image, batch.name + " depthImg");

      VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      if(depthStencilFormat == VK_FORMAT_D16_UNORM || depthStencilFormat == VK_FORMAT_D32_SFLOAT)
        depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;

      depthView = createImageView(
          vkh::ImageViewCreateInfo(batch.depthImg.image, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat,
                                   {}, vkh::ImageSubresourceRange(depthAspects, mip, 1, slice, 1)));
      setName(depthView, batch.name + " depthView");

      atts.push_back(depthView);

      renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
          depthStencilFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
          VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, sampleCount,
          VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE));
    }

    renderPassCreateInfo.addSubpass(cols,
                                    depthView == VK_NULL_HANDLE
                                        ? VK_ATTACHMENT_UNUSED
                                        : (uint32_t)renderPassCreateInfo.attachments.size() - 1,
                                    VK_IMAGE_LAYOUT_GENERAL);

    batch.rp = createRenderPass(renderPassCreateInfo);

    batch.extent.width >>= mip;
    batch.extent.height >>= mip;

    batch.fb = createFramebuffer(vkh::FramebufferCreateInfo(batch.rp, atts, batch.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.renderPass = batch.rp;

    pipeCreateInfo.layout = layout;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {vertexShader, fragmentShader};

    pipeCreateInfo.rasterizationState.depthClampEnable = VK_FALSE;
    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    pipeCreateInfo.multisampleState.rasterizationSamples = sampleCount;

    pipeCreateInfo.depthStencilState.depthTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;
    pipeCreateInfo.depthStencilState.front.passOp = VK_STENCIL_OP_REPLACE;
    pipeCreateInfo.depthStencilState.front.reference = 0x55;
    pipeCreateInfo.depthStencilState.front.compareMask = 0xff;
    pipeCreateInfo.depthStencilState.front.writeMask = 0xff;
    pipeCreateInfo.depthStencilState.back = pipeCreateInfo.depthStencilState.front;

    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    batch.depthWritePipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.depthWritePipe, batch.name + " depthWritePipe");

    {
      vkh::GraphicsPipelineCreateInfo depthPipeInfo = pipeCreateInfo;
      depthPipeInfo.depthStencilState.depthWriteEnable = VK_FALSE;
      depthPipeInfo.depthStencilState.depthTestEnable = VK_TRUE;
      depthPipeInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_EQUAL;

      batch.depthEqualPipe = createGraphicsPipeline(depthPipeInfo);
      setName(batch.depthEqualPipe, batch.name + " depthEqualPipe");
    }

    {
      vkh::GraphicsPipelineCreateInfo dynamicPipe = pipeCreateInfo;
      dynamicPipe.depthStencilState.depthWriteEnable = VK_FALSE;
      dynamicPipe.depthStencilState.depthTestEnable = VK_FALSE;

      dynamicPipe.dynamicState.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT};
      dynamicPipe.viewportState.scissors = {{{95 >> mip, 245 >> mip}, {20U >> mip, 8U >> mip}}};

      batch.fixedScissorPassPipe = createGraphicsPipeline(dynamicPipe);
      setName(batch.fixedScissorPassPipe, batch.name + " fixedScissorPassPipe");

      dynamicPipe.viewportState.scissors = {{{95 >> mip, 245 >> mip}, {8U >> mip, 8U >> mip}}};

      batch.fixedScissorFailPipe = createGraphicsPipeline(dynamicPipe);
      setName(batch.fixedScissorFailPipe, batch.name + " fixedScissorFailPipe");

      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

      batch.dynamicStencilRefPipe = createGraphicsPipeline(dynamicPipe);
      setName(batch.dynamicStencilRefPipe, batch.name + " dynamicStencilRefPipe");

      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);

      batch.dynamicStencilMaskPipe = createGraphicsPipeline(dynamicPipe);
      setName(batch.dynamicStencilMaskPipe, batch.name + " dynamicStencilMaskPipe");
    }

    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    {
      vkh::GraphicsPipelineCreateInfo depthPipeInfo = pipeCreateInfo;
      depthPipeInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
      depthPipeInfo.depthStencilState.depthBoundsTestEnable = VK_TRUE;
      batch.depthPipe = createGraphicsPipeline(depthPipeInfo);
      setName(batch.depthPipe, batch.name + " depthPipe");
    }

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    batch.stencilWritePipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.stencilWritePipe, batch.name + " stencilWritePipe");

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    batch.backgroundPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.backgroundPipe, batch.name + " backgroundPipe");

    pipeCreateInfo.stages = {vertexShader};
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.front.reference = 0x33;
    batch.noFsPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.noFsPipe, batch.name + " noFsPipe");
    pipeCreateInfo.stages = {vertexShader, fragmentShader};
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.front.reference = 0x55;

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_GREATER;
    batch.basicPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.basicPipe, batch.name + " basicPipe");
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;

    {
      vkh::GraphicsPipelineCreateInfo maskPipeInfo = pipeCreateInfo;
      maskPipeInfo.colorBlendState.attachments[0].colorWriteMask = 0x3;
      batch.colorMaskPipe = createGraphicsPipeline(maskPipeInfo);
      setName(batch.colorMaskPipe, batch.name + " colorMaskPipe");
    }

    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    batch.cullFrontPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.cullFrontPipe, batch.name + " cullFrontPipe");
    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    pipeCreateInfo.depthStencilState.depthBoundsTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.minDepthBounds = 0.0f;
    pipeCreateInfo.depthStencilState.maxDepthBounds = 1.0f;
    batch.depthBoundsPipe1 = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.depthBoundsPipe1, batch.name + " depthBoundsPipe1");
    pipeCreateInfo.depthStencilState.minDepthBounds = 0.4f;
    pipeCreateInfo.depthStencilState.maxDepthBounds = 0.6f;
    batch.depthBoundsPipe2 = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.depthBoundsPipe2, batch.name + " depthBoundsPipe2");
    pipeCreateInfo.depthStencilState.depthBoundsTestEnable = VK_FALSE;

    pipeCreateInfo.stages[1] =
        CompileShaderModule(common + defines + mspixel, ShaderLang::glsl, ShaderStage::frag, "main");

    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
    VkSampleMask not3 = 0x7;
    if(sampleCount == VK_SAMPLE_COUNT_4_BIT)
      pipeCreateInfo.multisampleState.pSampleMask = &not3;
    batch.sampleColourPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(batch.sampleColourPipe, batch.name + " sampleColourPipe");
  }

  void RunDraw(VkCommandBuffer cmd, const PixelHistory::draw &draw, uint32_t numInstances = 1)
  {
    vkCmdDraw(cmd, draw.count, numInstances, draw.first, 0);
  }

  void RunBatch(const VKTestBatch &b, VkCommandBuffer cmd)
  {
    float factor = float(b.extent.width) / float(mainWindow->scissor.extent.width);

    VkViewport v = {};
    v.width = (float)b.extent.width - (20.0f * factor);
    v.height = -(float)b.extent.height + (20.0f * factor);
    v.x = floorf(10.0f * factor);
    v.y = (float)b.extent.height - ceilf(10.0f * factor);
    v.maxDepth = 1.0f;

    VkRect2D scissor = {{0, 0}, b.extent};

    vkCmdSetViewport(cmd, 0, 1, &v);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // draw the setup triangles

    pushMarker(cmd, "Setup");
    {
      setMarker(cmd, "Depth Write");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthWritePipe);
      RunDraw(cmd, PixelHistory::DepthWrite);

      setMarker(cmd, "Depth Equal Setup");
      RunDraw(cmd, PixelHistory::DepthEqualSetup);

      setMarker(cmd, "Unbound Shader");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.noFsPipe);
      RunDraw(cmd, PixelHistory::UnboundPS);

      setMarker(cmd, "Stencil Write");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.stencilWritePipe);
      RunDraw(cmd, PixelHistory::StencilWrite);

      setMarker(cmd, "Background");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.backgroundPipe);
      RunDraw(cmd, PixelHistory::Background);

      setMarker(cmd, "Cull Front");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.cullFrontPipe);
      RunDraw(cmd, PixelHistory::CullFront);

      setMarker(cmd, "Depth Bounds Prep");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthBoundsPipe1);
      RunDraw(cmd, PixelHistory::DepthBoundsPrep);
      setMarker(cmd, "Depth Bounds Clip");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthBoundsPipe2);
      RunDraw(cmd, PixelHistory::DepthBoundsClip);
    }
    popMarker(cmd);

    pushMarker(cmd, "Stress Test");
    {
      pushMarker(cmd, "Lots of Drawcalls");
      {
        setMarker(cmd, "300 Draws");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthWritePipe);
        for(int d = 0; d < 300; ++d)
          RunDraw(cmd, PixelHistory::Draws300);
      }
      popMarker(cmd);

      setMarker(cmd, "300 Instances");
      RunDraw(cmd, PixelHistory::Instances300, 300);
    }
    popMarker(cmd);

    setMarker(cmd, "Simple Test");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.basicPipe);
    RunDraw(cmd, PixelHistory::MainTest);

    setMarker(cmd, "Scissor Fail");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.fixedScissorFailPipe);
    RunDraw(cmd, PixelHistory::ScissorFail);

    setMarker(cmd, "Scissor Pass");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.fixedScissorPassPipe);
    RunDraw(cmd, PixelHistory::ScissorPass);

    setMarker(cmd, "Stencil Ref");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.dynamicStencilRefPipe);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x67);
    RunDraw(cmd, PixelHistory::StencilRef);

    setMarker(cmd, "Stencil Mask");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.dynamicStencilMaskPipe);
    vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
    vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
    RunDraw(cmd, PixelHistory::StencilMask);

    setMarker(cmd, "Depth Test");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthPipe);
    vkCmdSetDepthBounds(cmd, 0.15f, 1.0f);
    RunDraw(cmd, PixelHistory::DepthTest);

    setMarker(cmd, "Sample Colouring");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.sampleColourPipe);
    RunDraw(cmd, PixelHistory::SampleColour);

    setMarker(cmd, "Depth Equal Fail");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthEqualPipe);
    RunDraw(cmd, PixelHistory::DepthEqualFail);

    setMarker(cmd, "Depth Equal Pass");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.depthEqualPipe);
    if(b.depthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
      RunDraw(cmd, PixelHistory::DepthEqualPass24);
    else if(b.depthFormat == VK_FORMAT_D16_UNORM_S8_UINT || b.depthFormat == VK_FORMAT_D16_UNORM)
      RunDraw(cmd, PixelHistory::DepthEqualPass16);
    else
      RunDraw(cmd, PixelHistory::DepthEqualPass32);

    setMarker(cmd, "Colour Masked");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.colorMaskPipe);
    RunDraw(cmd, PixelHistory::ColourMask);

    setMarker(cmd, "Overflowing");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.backgroundPipe);
    RunDraw(cmd, PixelHistory::OverflowingDraw);

    setMarker(cmd, "Per-Fragment discarding");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.backgroundPipe);
    RunDraw(cmd, PixelHistory::PerFragDiscard);

    setMarker(cmd, "No Output Shader");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.noFsPipe);
    RunDraw(cmd, PixelHistory::UnboundPS);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    PixelHistory::init();

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultA2V) * PixelHistory::vb.size(),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(PixelHistory::vb.data(), PixelHistory::vb.size() * sizeof(DefaultA2V));

    VkImage bbBlitSource = VK_NULL_HANDLE;

    struct DepthVariant
    {
      VkFormat fmt;
      std::string name;
      bool supported;
    };

    std::vector<DepthVariant> depthFormats = {{VK_FORMAT_D24_UNORM_S8_UINT, "D24S8", false},
                                              {VK_FORMAT_D16_UNORM_S8_UINT, "D16S8", false},
                                              {VK_FORMAT_D32_SFLOAT, "D32", false},
                                              {VK_FORMAT_D16_UNORM, "D16", false}};
    for(DepthVariant &fmt : depthFormats)
    {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(phys, fmt.fmt, &props);
      if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        fmt.supported = true;
    }

    // work around an NV bug with D32S8 when rendering to 3D textures is happening, prefer D24S8
    VkFormat defaultDepthFormat =
        depthFormats[0].supported ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;

    BuildTestBatch("Basic", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, defaultDepthFormat);
    bbBlitSource = batches.back().colImg.image;

    BuildTestBatch("MSAA", VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_R8G8B8A8_UNORM, defaultDepthFormat);
    BuildTestBatch("Colour Only", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                   VK_FORMAT_UNDEFINED);
    BuildTestBatch("Depth Only", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_UNDEFINED, defaultDepthFormat);
    BuildTestBatch("Mip & Slice", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                   defaultDepthFormat, 2, 3);
    BuildTestBatch("Slice MSAA", VK_SAMPLE_COUNT_4_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                   defaultDepthFormat, -1, 3);
    BuildTestBatch("Secondary", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, defaultDepthFormat);
    batches.back().secondary = true;

    BuildTestBatch("3D texture", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                   defaultDepthFormat, -1, -1, 8);

    for(DepthVariant &fmt : depthFormats)
    {
      if(fmt.supported)
        BuildTestBatch(fmt.name, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, fmt.fmt);
    }

    BuildTestBatch("F16 UNORM", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16B16A16_UNORM,
                   defaultDepthFormat);
    BuildTestBatch("F16 FLOAT", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16B16A16_SFLOAT,
                   defaultDepthFormat);
    BuildTestBatch("F32 FLOAT", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT,
                   defaultDepthFormat);

    BuildTestBatch("8-bit uint", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UINT, defaultDepthFormat);
    BuildTestBatch("16-bit uint", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16B16A16_UINT,
                   defaultDepthFormat);
    BuildTestBatch("32-bit uint", VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_UINT,
                   defaultDepthFormat);

    // for dispatch access, ensure all color images are in GENERAL as render passes for slice tests
    // will only transition that one slice
    for(const VKTestBatch &b : batches)
    {
      if(b.colImg.image == VK_NULL_HANDLE)
        continue;

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {vkh::ImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_MEMORY_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                        b.colImg.image)});

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

    while(Running())
    {
      std::vector<VkCommandBuffer> primaries;
      std::vector<VkCommandBuffer> secondaries;

      for(const VKTestBatch &b : batches)
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

        // set clears correctly for depth-only batches, the extra clear value on colour only is ignored
        std::vector<VkClearValue> clears;
        if(b.colImg.image == VK_NULL_HANDLE)
        {
          clears = {vkh::ClearValue(1.0f, 0)};
        }
        else
        {
          clears = {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f), vkh::ClearValue(1.0f, 0)};
          if(b.uint)
          {
            clears[0].color.uint32[0] = 80;
            clears[0].color.uint32[1] = 80;
            clears[0].color.uint32[2] = 80;
            clears[0].color.uint32[3] = 16;
          }
        }

        if(b.secondary)
        {
          pushMarker(cmd, "Batch: " + b.name);
          {
            setMarker(cmd, "Begin RenderPass");
            vkCmdBeginRenderPass(cmd,
                                 vkh::RenderPassBeginInfo(b.rp, b.fb, {{0, 0}, b.extent}, clears),
                                 VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

            VkCommandBuffer cmd2 = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
            secondaries.push_back(cmd2);
            vkBeginCommandBuffer(
                cmd2, vkh::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                                                  vkh::CommandBufferInheritanceInfo(b.rp, 0, b.fb)));
            vkh::cmdBindVertexBuffers(cmd2, 0, {vb.buffer}, {0});
            RunBatch(b, cmd2);
            vkEndCommandBuffer(cmd2);

            vkCmdExecuteCommands(cmd, 1, &cmd2);

            vkCmdEndRenderPass(cmd);
          }
          popMarker(cmd);
        }
        else
        {
          pushMarker(cmd, "Batch: " + b.name);
          {
            setMarker(cmd, "Begin RenderPass");

            vkCmdBeginRenderPass(cmd,
                                 vkh::RenderPassBeginInfo(b.rp, b.fb, {{0, 0}, b.extent}, clears),
                                 VK_SUBPASS_CONTENTS_INLINE);

            RunBatch(b, cmd);

            vkCmdEndRenderPass(cmd);

            setMarker(cmd, "Compute write");
            if(b.comppipe != VK_NULL_HANDLE)
            {
              vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, b.comppipe);
              vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, b.compLayout, 0,
                                         {b.compset}, {});
              vkCmdDispatch(cmd, 1, 1, 1);
            }
          }
          popMarker(cmd);
        }

        vkEndCommandBuffer(cmd);

        primaries.push_back(cmd);
      }

      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        if(bbBlitSource != VK_NULL_HANDLE)
        {
          vkh::cmdPipelineBarrier(
              cmd, {vkh::ImageMemoryBarrier(
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                       VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                       VK_IMAGE_LAYOUT_GENERAL, bbBlitSource)});
          blitToSwap(cmd, bbBlitSource, VK_IMAGE_LAYOUT_GENERAL,
                     mainWindow->GetImage(mainWindow->imgIndex), VK_IMAGE_LAYOUT_GENERAL);
        }

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkEndCommandBuffer(cmd);

        primaries.push_back(cmd);
      }

      Submit(0, 1, primaries, secondaries);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
