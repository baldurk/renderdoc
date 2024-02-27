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

#include "vk_test.h"

RD_TEST(VK_Discard_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests the different discard patterns possible on replay.";

  const std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 0, std140) uniform constsbuf
{
  vec4 value;
};

void main()
{
	Color = vec4(1.0f, 0.0f, 0.0f, 1.0f);

  if(value.y == 234.0f)
    Color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
}

)EOSHADER";

  AllocatedBuffer emptyBuf;

  void Clear(VkCommandBuffer cmd, const AllocatedImage &img)
  {
    if(img.image == VK_NULL_HANDLE)
      return;

    vkh::ImageSubresourceRange range;

    if(img.createInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
       img.createInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(img.createInfo.format == VK_FORMAT_D32_SFLOAT ||
            img.createInfo.format == VK_FORMAT_D16_UNORM)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if(img.createInfo.format == VK_FORMAT_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT |
                                             VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL, img.image, range),
             });

    VkClearDepthStencilValue val = {0.4f, 0x40};

    if(img.createInfo.format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC2_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC3_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC4_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC5_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC6H_UFLOAT_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC7_UNORM_BLOCK)
    {
      // can't clear compressed formats with vkCmdClearColorImage
      VkBufferImageCopy region = {};
      std::vector<VkBufferImageCopy> regions;

      region.imageSubresource.aspectMask = range.aspectMask;
      region.imageSubresource.layerCount = img.createInfo.arrayLayers;

      for(uint32_t m = 0; m < img.createInfo.mipLevels; m++)
      {
        region.imageExtent.width = std::max(1U, img.createInfo.extent.width >> m);
        region.imageExtent.height = std::max(1U, img.createInfo.extent.height >> m);
        region.imageExtent.depth = std::max(1U, img.createInfo.extent.depth >> m);
        region.imageSubresource.mipLevel = m;

        regions.push_back(region);
      }

      vkCmdCopyBufferToImage(cmd, emptyBuf.buffer, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             (uint32_t)regions.size(), regions.data());
      return;
    }

    if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
    {
      if(img.createInfo.format == VK_FORMAT_A2B10G10R10_UINT_PACK32)
        vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0u, 1023u, 0u, 1u), 1, range);
      else
        vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.0f, 1.0f, 0.0f, 1.0f), 1, range);
    }
    else
    {
      vkCmdClearDepthStencilImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL, &val, 1, range);
    }
  }

  void DiscardImage(VkCommandBuffer cmd, const AllocatedImage &img,
                    vkh::ImageSubresourceRange range = {})
  {
    if(img.createInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
       img.createInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(img.createInfo.format == VK_FORMAT_D32_SFLOAT ||
            img.createInfo.format == VK_FORMAT_D16_UNORM)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if(img.createInfo.format == VK_FORMAT_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                         img.image, range),
             });
  }

  AllocatedImage MakeTex2D(VkFormat fmt, uint32_t width, uint32_t height, uint32_t mips = 1,
                           uint32_t arraySlices = 1)
  {
    return AllocatedImage(this,
                          vkh::ImageCreateInfo(width, height, 0, fmt,
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT, mips, arraySlices),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
  }

  AllocatedImage MakeTex2DMS(VkFormat fmt, uint32_t width, uint32_t height, uint32_t samples,
                             uint32_t arraySlices = 1)
  {
    bool depth = (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT ||
                  fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D16_UNORM ||
                  fmt == VK_FORMAT_S8_UINT);

    return AllocatedImage(this,
                          vkh::ImageCreateInfo(width, height, 0, fmt,
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                   (depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                          : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
                                               1, arraySlices, (VkSampleCountFlagBits)samples),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
  }

  void Prepare(int argc, char **argv)
  {
    optDevExts.push_back(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR separateDepthStencilFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR,
    };

    if(std::find(devExts.begin(), devExts.end(),
                 VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME) != devExts.end())
    {
      getPhysFeatures2(&separateDepthStencilFeatures);

      if(!separateDepthStencilFeatures.separateDepthStencilLayouts)
        Avail = "'separateDepthStencilLayouts' not available";

      devInfoNext = &separateDepthStencilFeatures;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    bool d32s8 = true;
    VkFormat depthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(phys, depthStencilFormat, &props);
    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
    {
      depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
      d32s8 = false;
    }

    bool d24s8 = true;
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_D24_UNORM_S8_UINT, &props);
    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
      d24s8 = false;

    bool d16 = true;
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_D16_UNORM, &props);
    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
      d16 = false;

    bool d32 = true;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    vkGetPhysicalDeviceFormatProperties(phys, depthFormat, &props);
    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
    {
      depthFormat = VK_FORMAT_X8_D24_UNORM_PACK32;
      d32 = false;
    }

    bool s8 = true;
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_S8_UINT, &props);
    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
      s8 = false;

    bool KHR_separate_stencil =
        std::find(devExts.begin(), devExts.end(),
                  VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME) != devExts.end();

    {
      byte *empty = new byte[16 * 1024 * 1024];
      memset(empty, 0x88, 16 * 1024 * 1024);

      emptyBuf = AllocatedBuffer(
          this, vkh::BufferCreateInfo(16 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

      emptyBuf.upload(empty, 16 * 1024 * 1024);
      delete[] empty;
    }

    const VmaAllocationCreateInfo gpu = VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY});

    AllocatedImage ignoreimg(
        this,
        vkh::ImageCreateInfo(screenWidth, screenHeight, 0, VK_FORMAT_R16G16B16A16_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView ignoreview = createImageView(vkh::ImageViewCreateInfo(
        ignoreimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT));

    setName(ignoreimg.image, "NoDiscard");

    // create RP color image
    AllocatedImage colimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R16G16B16A16_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView colview = createImageView(vkh::ImageViewCreateInfo(
        colimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT));

    setName(colimg.image, "RPColor");

    // create depth-stencil image
    AllocatedImage depthimg(
        this,
        vkh::ImageCreateInfo(
            mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0, depthStencilFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView depthview = createImageView(vkh::ImageViewCreateInfo(
        depthimg.image, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)));

    setName(depthimg.image, "RPDepth");

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        depthStencilFormat, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})}, 1,
                                    VK_IMAGE_LAYOUT_GENERAL);

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    renderPassCreateInfo.attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassCreateInfo.attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    renderPassCreateInfo.attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassCreateInfo.attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassCreateInfo.attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassCreateInfo.attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassCreateInfo.attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassCreateInfo.attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassCreateInfo.attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassCreateInfo.attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderPass undefLoadRP = createRenderPass(renderPassCreateInfo);

    renderPassCreateInfo.attachments.resize(1);
    renderPassCreateInfo.attachments[0].format = mainWindow->format;
    renderPassCreateInfo.attachments[0].samples = VK_SAMPLE_COUNT_4_BIT;

    renderPassCreateInfo.subpasses[0].pDepthStencilAttachment = NULL;

    VkRenderPass msaaRP = createRenderPass(renderPassCreateInfo);

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    pipeCreateInfo.layout = layout;

    pipeCreateInfo.stages = {
        CompileShaderModule(VKFullscreenQuadVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

    pipeCreateInfo.renderPass = msaaRP;
    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedImage msaaimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, 1,
                             VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView msaaRTV = createImageView(
        vkh::ImageViewCreateInfo(msaaimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format));

    VkFramebuffer msaaFB = createFramebuffer(vkh::FramebufferCreateInfo(
        msaaRP, {msaaRTV}, {mainWindow->scissor.extent.width, mainWindow->scissor.extent.height}));

    Vec4f cbufferdata[64] = {};
    cbufferdata[0] = Vec4f(0.0f, 234.0f, 0.0f, 0.0f);

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    cb.upload(cbufferdata);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(cb.buffer)}),
                });

    VkFramebuffer fb = createFramebuffer(vkh::FramebufferCreateInfo(
        renderPass, {colview, depthview, ignoreview}, mainWindow->scissor.extent));

    AllocatedImage tex1d(this,
                         vkh::ImageCreateInfo(300, 0, 0, VK_FORMAT_R16G16B16A16_SFLOAT,
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT, 3, 5),
                         VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    AllocatedImage tex3d(this,
                         vkh::ImageCreateInfo(300, 300, 15, VK_FORMAT_R16G16B16A16_SFLOAT,
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT, 3),
                         VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    AllocatedImage tex1dsub(this,
                            vkh::ImageCreateInfo(300, 0, 0, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT, 3, 5),
                            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    AllocatedImage tex3dsub(this,
                            vkh::ImageCreateInfo(300, 300, 15, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT, 3),
                            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(tex1d.image, "Tex1D: DiscardAll");
    setName(tex3d.image, "Tex3D: DiscardAll");
    setName(tex1dsub.image, "Tex1D: DiscardAll Mip1 Slice1,2");
    setName(tex3dsub.image, "Tex3D: DiscardAll Mip1");

    std::vector<AllocatedImage> texs;

#define TEX_TEST(name, x)                                                          \
  if(first)                                                                        \
  {                                                                                \
    texs.push_back(x);                                                             \
    Clear(cmd, texs.back());                                                       \
    setName(texs.back().image, "Tex" + std::to_string(texs.size()) + ": " + name); \
  }                                                                                \
  tex = texs[t++];

    bool first = true;

    while(Running())
    {
      if(!first)
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        pushMarker(cmd, "Clears");

        for(AllocatedImage t : texs)
          Clear(cmd, t);

        Clear(cmd, tex1d);
        Clear(cmd, tex3d);
        Clear(cmd, tex1dsub);
        Clear(cmd, tex3dsub);

        popMarker(cmd);

        vkEndCommandBuffer(cmd);

        Submit(999, 999, {cmd});
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      // bind descriptor sets here, these should not be disturbed by any discard patterns
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);

      Clear(cmd, ignoreimg);
      Clear(cmd, colimg);
      Clear(cmd, depthimg);

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_GENERAL, swapimg),
               });

      // this is an anchor point for us to jump to and observe textures with all cleared contents
      // and no discard patterns
      setMarker(cmd, "TestStart");
      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      int t = 0;
      AllocatedImage tex;

      // test a few different formats
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R16G16B16A16_SFLOAT, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_A2B10G10R10_UNORM_PACK32, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_A2B10G10R10_UINT_PACK32, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R8G8B8A8_UNORM, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC2_UNORM_BLOCK, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC3_UNORM_BLOCK, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC4_UNORM_BLOCK, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC5_UNORM_BLOCK, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC6H_UFLOAT_BLOCK, 300, 300));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC7_UNORM_BLOCK, 300, 300));
      DiscardImage(cmd, tex);

      // test with different mips/array sizes
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R16G16B16A16_SFLOAT, 300, 300, 5));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R16G16B16A16_SFLOAT, 300, 300, 1, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R16G16B16A16_SFLOAT, 300, 300, 5, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R16G16B16A16_SFLOAT, 30, 5));
      DiscardImage(cmd, tex);

      // test MSAA textures
      TEX_TEST("DiscardAll", MakeTex2DMS(VK_FORMAT_R16G16B16A16_SFLOAT, 300, 300, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(VK_FORMAT_R16G16B16A16_SFLOAT, 300, 300, 4, 5));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(VK_FORMAT_R16G16B16A16_UINT, 300, 300, 4, 5));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(VK_FORMAT_R16G16B16A16_SINT, 300, 300, 4, 5));
      DiscardImage(cmd, tex);

      // test depth textures
      if(d32)
      {
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_D32_SFLOAT, 300, 300));
        DiscardImage(cmd, tex);
      }

      if(d32s8)
      {
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_D32_SFLOAT_S8_UINT, 300, 300));
        DiscardImage(cmd, tex);
      }

      if(d24s8)
      {
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_D24_UNORM_S8_UINT, 300, 300));
        DiscardImage(cmd, tex);
      }

      if(d16)
      {
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_D16_UNORM, 300, 300));
        DiscardImage(cmd, tex);
      }

      if(s8)
      {
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_S8_UINT, 300, 300, 5));
        DiscardImage(cmd, tex);
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_S8_UINT, 300, 300, 1, 4));
        DiscardImage(cmd, tex);
        TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_S8_UINT, 300, 300, 5, 4));
        DiscardImage(cmd, tex);
        TEX_TEST("DiscardAll", MakeTex2DMS(VK_FORMAT_S8_UINT, 300, 300, 4));
        DiscardImage(cmd, tex);
        TEX_TEST("DiscardAll", MakeTex2DMS(VK_FORMAT_S8_UINT, 300, 300, 4, 2));
        DiscardImage(cmd, tex);
      }

      TEX_TEST("DiscardAll", MakeTex2D(depthFormat, 300, 300, 5));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthFormat, 300, 300, 1, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthFormat, 300, 300, 5, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthStencilFormat, 300, 300, 5));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthStencilFormat, 300, 300, 1, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthStencilFormat, 300, 300, 5, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(depthStencilFormat, 300, 300, 4));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(depthStencilFormat, 300, 300, 4, 5));
      DiscardImage(cmd, tex);

      // test large textures
      uint32_t largeDim = 4096;
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_R16G16B16A16_SFLOAT, largeDim, largeDim));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(VK_FORMAT_BC2_UNORM_BLOCK, largeDim, largeDim));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthFormat, largeDim, largeDim));
      DiscardImage(cmd, tex);
      TEX_TEST("DiscardAll", MakeTex2D(depthStencilFormat, largeDim, largeDim));
      DiscardImage(cmd, tex);

      // if supported, test invalidating depth and stencil alone
      if(KHR_separate_stencil)
      {
        TEX_TEST("DiscardAll DepthOnly", MakeTex2D(depthStencilFormat, 300, 300));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR,
                         tex.image, vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)),
                 });

        TEX_TEST("DiscardAll StencilOnly", MakeTex2D(depthStencilFormat, 300, 300));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR,
                         tex.image, vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT)),
                 });
      }

      // test 1D/3D textures
      DiscardImage(cmd, tex1d);
      DiscardImage(cmd, tex3d);

      DiscardImage(cmd, tex1dsub, vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 1, 2));
      DiscardImage(cmd, tex3dsub, vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, 1));

      // test a renderpass. This tests rects via renderArea, as well as vulkan-specific load-op and
      // store-op and ensures that unused attachments are not discarded

      VkRect2D sc = {{50, 50}, {75, 75}};

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, fb, sc),
                           VK_SUBPASS_CONTENTS_INLINE);

      // add an anchor for us to check mid-render pass. This clear only sets one pixel to black
      // which won't affect our tests
      setMarker(cmd, "TestMiddle");
      VkClearAttachment att = {VK_IMAGE_ASPECT_COLOR_BIT, 0, vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)};
      VkClearRect rect = {vkh::Rect2D({50, 50}, {1, 1}), 0, 1};
      vkCmdClearAttachments(cmd, 1, &att, 1, &rect);

      vkCmdEndRenderPass(cmd);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, swapimg),
          });

      setMarker(cmd, "TestEnd");
      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      // make sure a renderpass with UNDEFINED initialLayout and LOAD_OP_LOAD still gets an
      // undefined pattern.

      // first re-clear the attachments
      Clear(cmd, ignoreimg);
      Clear(cmd, colimg);
      setMarker(cmd, "UndefinedLoad_Before");
      Clear(cmd, depthimg);

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(undefLoadRP, fb, sc),
                           VK_SUBPASS_CONTENTS_INLINE);
      vkCmdEndRenderPass(cmd);

      setMarker(cmd, "UndefinedLoad_After");

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(msaaRP, msaaFB, mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdDraw(cmd, 4, 1, 0, 0);
      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();

      first = false;
    }

    return 0;
  }
};

REGISTER_TEST();
