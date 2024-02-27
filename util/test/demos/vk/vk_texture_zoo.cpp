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

using namespace TextureZoo;

RD_TEST(VK_Texture_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests all possible combinations of texture type and format that are supported.";

  std::string blitVertex = R"EOSHADER(
#version 420 core

void main()
{
  const vec4 verts[4] = vec4[4](vec4(-1.0, -1.0, 0.5, 1.0), vec4(3.0, -1.0, 0.5, 1.0),
                                vec4(-1.0, 3.0, 0.5, 1.0), vec4(1.0, 1.0, 0.5, 1.0));

  gl_Position = verts[gl_VertexIndex];
}

)EOSHADER";

  std::string pixelTemplate = R"EOSHADER(
#version 420 core

layout(binding = 0) uniform &texdecl intex;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vec4(texelFetch(intex, &params));
}
)EOSHADER";

  std::string pixelMSFloat = R"EOSHADER(
#version 420 core

layout(push_constant) uniform PushData {
  uint slice;
  uint mip;
  uint flags;
  uint zlayer;
} push;

float srgb2linear(float f)
{
  if (f <= 0.04045f)
    return f / 12.92f;
  else
    return pow((f + 0.055f) / 1.055f, 2.4f);
}

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  vec4 ret = vec4(0.1f, 0.35f, 0.6f, 0.85f);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + push.zlayer) % max(1u, TEX_WIDTH >> push.mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(push.slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += 0.075f.xxxx * (gl_SampleID + push.mip);

  // Signed normals are negative
  if((push.flags & 1) != 0)
    ret = -ret;

  // undo SRGB curve applied in output merger, to match the textures we just blat values into
  // without conversion (which are then interpreted as srgb implicitly)
  if((push.flags & 2) != 0)
  {
    ret.r = srgb2linear(ret.r);
    ret.g = srgb2linear(ret.g);
    ret.b = srgb2linear(ret.b);
  }

  // BGR flip - same as above, for BGRA textures
  if((push.flags & 4) != 0)
    ret.rgb = ret.bgr;

   // put red into alpha, because that's what we did in manual upload
  if((push.flags & 8) != 0)
    ret.a = ret.r;

  Color = ret;
}

)EOSHADER";

  std::string pixelMSDepth = R"EOSHADER(
#version 420 core

layout(push_constant) uniform PushData {
  uint slice;
  uint mip;
  uint flags;
  uint zlayer;
} push;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  float ret = 0.1f;

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + push.zlayer) % max(1u, TEX_WIDTH >> push.mip);

  // pixels off the diagonal invert the colors
  // second slice adds a coarse checkerboard pattern of inversion
  if((offs_x != y) != (push.slice > 0 && (((x / 2) % 2) != ((y / 2) % 2))))
  {
    ret = 0.85f;

    // so we can fill stencil data, clip off the inverted values
    if(push.flags == 1)
      discard;
  }

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += 0.075f * (gl_SampleID + push.mip);

  gl_FragDepth = ret;
}

)EOSHADER";

  std::string pixelMSUInt = R"EOSHADER(
#version 420 core

layout(push_constant) uniform PushData {
  uint slice;
  uint mip;
  uint flags;
  uint zlayer;
} push;

layout(location = 0, index = 0) out uvec4 Color;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  uvec4 ret = uvec4(10, 40, 70, 100);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + push.zlayer) % max(1u, TEX_WIDTH >> push.mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(push.slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // BGR flip - to match the textures we just blat values into
  // without conversion (which are then interpreted as bgra implicitly)
  if((push.flags & 4) != 0)
    ret.rgb = ret.bgr;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += uvec4(10, 10, 10, 10) * (gl_SampleID + push.mip);

  Color = ret;
}

)EOSHADER";

  std::string pixelMSSInt = R"EOSHADER(
#version 420 core

layout(push_constant) uniform PushData {
  uint slice;
  uint mip;
  uint flags;
  uint zlayer;
} push;

layout(location = 0, index = 0) out ivec4 Color;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  ivec4 ret = ivec4(10, 40, 70, 100);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + push.zlayer) % max(1u, TEX_WIDTH >> push.mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(push.slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // BGR flip - to match the textures we just blat values into
  // without conversion (which are then interpreted as bgra implicitly)
  if((push.flags & 4) != 0)
    ret.rgb = ret.bgr;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += ivec4(10 * (gl_SampleID + push.mip));

  Color = -ret;
}

)EOSHADER";

  struct VKFormat
  {
    const std::string name;
    VkFormat texFmt;
    VkFormat viewFmt;
    TexConfig cfg;
  };

  struct TestCase
  {
    VKFormat fmt;
    uint32_t dim;
    bool isArray;
    bool canRender;
    bool isDepth;
    bool isMSAA;
    bool hasData;
    AllocatedImage res;
    VkImageView view;
    VkDescriptorSet set;
    VkImageViewType viewType;
  };

  std::string MakeName(const TestCase &test)
  {
    std::string name = "Texture " + std::to_string(test.dim) + "D";

    if(test.isMSAA)
      name += " MSAA";
    if(test.isArray)
      name += " Array";

    return name;
  }

  VkPipelineLayout layout;
  VkPipelineShaderStageCreateInfo vs;
  VkRenderPass rp;

  VkPipeline GetPSO(const TestCase &test)
  {
    static std::map<uint32_t, VkPipeline> PSOs;

    uint32_t key = uint32_t(test.fmt.cfg.data);
    key |= test.dim << 6;
    key |= test.isMSAA ? 0x80000 : 0;
    key |= test.isArray ? 0x100000 : 0;

    VkPipeline ret = PSOs[key];
    if(!ret)
    {
      std::string texType = "sampler" + std::to_string(test.dim) + "D";
      if(test.isMSAA)
        texType += "MS";
      if(test.dim < 3 && test.isArray)
        texType += "Array";

      std::string typemod = "";

      if(test.fmt.cfg.data == DataType::UInt)
        typemod = "u";
      else if(test.fmt.cfg.data == DataType::SInt)
        typemod = "i";

      std::string src = pixelTemplate;

      uint32_t dim = test.dim + (test.isArray ? 1 : 0);

      if(dim == 1)
        src.replace(src.find("&params"), 7, "int(0), 0");
      else if(dim == 2)
        src.replace(src.find("&params"), 7, "ivec2(0), 0");
      else if(dim == 3)
        src.replace(src.find("&params"), 7, "ivec3(0), 0");

      src.replace(src.find("&texdecl"), 8, typemod + texType);

      vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

      pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

      pipeCreateInfo.layout = layout;
      pipeCreateInfo.renderPass = rp;

      pipeCreateInfo.stages = {
          vs,
          CompileShaderModule(src, ShaderLang::glsl, ShaderStage::frag, "main"),
      };

      ret = PSOs[key] = createGraphicsPipeline(pipeCreateInfo);
    }

    return ret;
  }

  VkDeviceSize CurOffset = 0;
  AllocatedBuffer uploadBuf;
  byte *CurBuf = NULL;

  bool SetData(VkCommandBuffer cmd, const TestCase &test)
  {
    uint32_t slices = test.isArray ? texSlices : 1;
    uint32_t mips = test.isMSAA ? 1 : texMips;

    Vec4i dim(texWidth, texHeight, texDepth);

    if(test.dim < 3)
      dim.z = 1;
    if(test.dim < 2)
      dim.y = 1;

    TexData data;

    for(uint32_t s = 0; s < slices; s++)
    {
      for(uint32_t m = 0; m < mips; m++)
      {
        MakeData(data, test.fmt.cfg, dim, m, s);

        if(data.byteData.empty())
          return false;

        if(s == 0 && m == 0)
        {
          vkh::cmdPipelineBarrier(
              cmd,
              {
                  vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, test.res.image),
              });
        }

        VkBufferImageCopy copy = {};
        copy.bufferOffset = CurOffset;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageExtent.width = std::max(1, dim.x >> m);
        copy.imageExtent.height = std::max(1, dim.y >> m);
        copy.imageExtent.depth = std::max(1, dim.z >> m);
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.baseArrayLayer = s;
        copy.imageSubresource.layerCount = 1;
        copy.imageSubresource.mipLevel = m;

        memcpy(CurBuf + CurOffset, data.byteData.data(), data.byteData.size());

        CurOffset += data.byteData.size();

        CurOffset = AlignUp(CurOffset, (VkDeviceSize)256);

        vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, test.res.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      }
    }

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, test.res.image),
             });

    return true;
  }

  VkDescriptorSetLayout setlayout;

  TestCase FinaliseTest(VkCommandBuffer cmd, TestCase test)
  {
    bool mutableFmt = (test.fmt.texFmt != test.fmt.viewFmt);
    VkImageCreateFlags flags = mutableFmt ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if(test.isDepth)
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else if(test.canRender)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t w = texWidth, h = texHeight, d = texDepth;
    if(test.dim < 3)
      d = 0;
    if(test.dim < 2)
      h = 0;

    if(test.dim == 1)
    {
      h = d = 0;
      test.viewType = test.isArray ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
    }
    else if(test.dim == 2)
    {
      d = 0;
      test.viewType = test.isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    }
    else if(test.dim == 3)
    {
      // need this so we can render to slices
      flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
      test.viewType = VK_IMAGE_VIEW_TYPE_3D;
    }

    VkImageAspectFlags viewAspect = VK_IMAGE_ASPECT_COLOR_BIT;

    if(test.isDepth)
    {
      if(test.fmt.cfg.data == DataType::UInt)
        viewAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
      else
        viewAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageFormatListCreateInfoKHR formatList = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR};
    formatList.viewFormatCount = 2;
    VkFormat fmts[] = {test.fmt.texFmt, test.fmt.viewFmt};
    formatList.pViewFormats = fmts;

    test.res = AllocatedImage(
        this,
        vkh::ImageCreateInfo(
            w, h, d, test.fmt.texFmt, usage, test.isMSAA ? 1 : texMips, test.isArray ? texSlices : 1,
            test.isMSAA ? VkSampleCountFlagBits(texSamples) : VK_SAMPLE_COUNT_1_BIT, flags)
            .next(hasExt(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME) && mutableFmt ? &formatList : NULL),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    test.view = createImageView(vkh::ImageViewCreateInfo(
        test.res.image, test.viewType, test.fmt.viewFmt, {}, vkh::ImageSubresourceRange(viewAspect)));
    test.set = allocateDescriptorSet(setlayout);
    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(test.set, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            {vkh::DescriptorImageInfo(test.view)}),
                });

    setName(test.res.image, MakeName(test) + " " + test.fmt.name);

    if(!test.isMSAA)
    {
      pushMarker(cmd, "Set data for " + test.fmt.name + " " + MakeName(test));

      test.hasData = SetData(cmd, test);

      popMarker(cmd);
    }

    return test;
  }

  void AddSupportedTests(const VKFormat &f, std::vector<TestCase> &test_textures, bool depthMode)
  {
    VkCommandBuffer cmd = GetCommandBuffer();

    vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

    CurOffset = 0;

    VkFormatProperties props = {}, props2 = {};
    vkGetPhysicalDeviceFormatProperties(phys, f.texFmt, &props);
    vkGetPhysicalDeviceFormatProperties(phys, f.viewFmt, &props2);

    // only check what is supported by both formats
    props.optimalTilingFeatures &= props2.optimalTilingFeatures;

    bool viewCast = (f.texFmt != f.viewFmt);

    bool renderable = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
    bool depth = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if(renderable)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(depth)
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageFormatProperties props1D = {}, props2D = {}, props3D = {};
    VkResult vkr = VK_SUCCESS;
    vkr = vkGetPhysicalDeviceImageFormatProperties(
        phys, f.viewFmt, VK_IMAGE_TYPE_1D, VK_IMAGE_TILING_OPTIMAL, usage,
        viewCast ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0, &props1D);
    if(vkr != VK_SUCCESS)
      props1D = {};
    vkr = vkGetPhysicalDeviceImageFormatProperties(
        phys, f.viewFmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage,
        viewCast ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0, &props2D);
    if(vkr != VK_SUCCESS)
      props2D = {};
    vkr = vkGetPhysicalDeviceImageFormatProperties(
        phys, f.viewFmt, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL, usage,
        VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT | (viewCast ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0),
        &props3D);
    if(vkr != VK_SUCCESS)
      props3D = {};

    // rendering to depth 3D textures is broken on NV, fixed in a future driver version (guess made,
    // will be updated once fix ships)
    if(depth && physProperties.vendorID == PCI_VENDOR_NV &&
       physProperties.driverVersion < VK_MAKE_VERSION_NV(445, 0, 0, 0))
      props3D = {};

    if(!renderable && !depth)
      props2D.sampleCounts = VK_SAMPLE_COUNT_1_BIT;

    if((props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) || depth)
    {
      // TODO: disable 1D depth textures for now, we don't support displaying them
      if(!depthMode)
      {
        if(props1D.maxExtent.width >= texWidth)
        {
          test_textures.push_back(FinaliseTest(cmd, {f, 1, false, renderable, depth}));
          test_textures.push_back(FinaliseTest(cmd, {f, 1, true, renderable, depth}));
        }
        else
        {
          test_textures.push_back({f, 1, false});
          test_textures.push_back({f, 1, true});
        }
      }

      if(props2D.maxExtent.width >= texWidth)
      {
        test_textures.push_back(FinaliseTest(cmd, {f, 2, false, renderable, depth}));
        test_textures.push_back(FinaliseTest(cmd, {f, 2, true, renderable, depth}));
      }
      else
      {
        test_textures.push_back({f, 2, false});
        test_textures.push_back({f, 2, true});
      }

      if(props3D.maxExtent.width >= texWidth)
      {
        test_textures.push_back(FinaliseTest(cmd, {f, 3, false, renderable, depth}));
      }
      else
      {
        test_textures.push_back({f, 3, false});
      }

      // TODO: we don't support MSAA<->Array copies for these odd sized pixels, and I suspect
      // drivers don't tend to support the formats anyway. Disable for now
      if((f.cfg.type != TextureType::Regular || f.cfg.componentCount != 3) &&
         props2D.sampleCounts & texSamples)
      {
        test_textures.push_back(FinaliseTest(cmd, {f, 2, false, true, depth, true}));
        test_textures.push_back(FinaliseTest(cmd, {f, 2, true, true, depth, true}));
      }
      else
      {
        test_textures.push_back({f, 2, false, true, depth, true});
        test_textures.push_back({f, 2, true, true, depth, true});
      }
    }
    else
    {
      test_textures.push_back({f, 2, false});

      if(props1D.maxExtent.width >= texWidth || props2D.maxExtent.width >= texWidth ||
         props3D.maxExtent.width >= texWidth)
      {
        TEST_WARN("Format %d can't be loaded in shader but can be a texture!", f.texFmt);
      }
    }

    vkEndCommandBuffer(cmd);

    Submit(99, 99, {cmd});
    vkDeviceWaitIdle(device);
  }

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);

    features.sampleRateShading = true;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkSampler sampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_NEAREST));

    setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler},
    }));

    layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setlayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_ALL, 0, 16)}));

    vs = CompileShaderModule(blitVertex, ShaderLang::glsl, ShaderStage::vert, "main");

    AllocatedImage fltTex(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView fltView = createImageView(vkh::ImageViewCreateInfo(
        fltTex.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass(
        {VkAttachmentReference({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL})});

    renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency(
        0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT));

    rp = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer =
        createFramebuffer(vkh::FramebufferCreateInfo(rp, {fltView}, mainWindow->scissor.extent));

    uploadBuf = AllocatedBuffer(
        this, vkh::BufferCreateInfo(8 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    CurBuf = uploadBuf.map();

#define TEST_CASE_NAME(texFmt, viewFmt)             \
  (texFmt == viewFmt) ? std::string(&(#texFmt)[10]) \
                      : (std::string(&(#texFmt)[10]) + "->" + (strchr(&(#viewFmt)[10], '_') + 1))

#define TEST_CASE(texType, texFmt, viewFmt, compCount, byteWidth, dataType) \
  {                                                                         \
      TEST_CASE_NAME(texFmt, viewFmt),                                      \
      texFmt,                                                               \
      viewFmt,                                                              \
      {texType, compCount, byteWidth, dataType},                            \
  }

    std::vector<TestCase> test_textures;

    const VKFormat color_tests[] = {
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SFLOAT,
                  4, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_UINT,
                  4, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32A32_SFLOAT,
                  VK_FORMAT_R32G32B32A32_SFLOAT, 4, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_UINT, 4,
                  4, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SINT, 4,
                  4, DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT, 3, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_UINT, 3, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, 3,
                  4, DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_UINT, 3, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32_SINT, 3, 4,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_SFLOAT, 2, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32_UINT, 2, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, 2, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_UINT, 2, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32_SINT, 2, 4,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R32_UINT, VK_FORMAT_R32_SFLOAT, 1, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_UINT, 1, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT, 1, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32_UINT, VK_FORMAT_R32_UINT, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R32_SINT, VK_FORMAT_R32_SINT, 1, 4, DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_SFLOAT,
                  4, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_UINT,
                  4, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_UNORM,
                  4, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_SFLOAT,
                  VK_FORMAT_R16G16B16A16_SFLOAT, 4, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM,
                  4, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_UINT, 4,
                  2, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_SNORM,
                  4, 2, DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SINT, 4,
                  2, DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_USCALED,
                  VK_FORMAT_R16G16B16A16_USCALED, 4, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16A16_SSCALED,
                  VK_FORMAT_R16G16B16A16_SSCALED, 4, 2, DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16_SFLOAT, 3, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16_UINT, 3, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16_UNORM, 3, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, 3,
                  2, DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16_UNORM, 3, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16_UINT, 3, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16_SNORM, 3, 2,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_SINT, VK_FORMAT_R16G16B16_SINT, 3, 2,
                  DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_USCALED, VK_FORMAT_R16G16B16_USCALED, 3,
                  2, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16B16_SSCALED, VK_FORMAT_R16G16B16_SSCALED, 3,
                  2, DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16_SFLOAT, 2, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16_UINT, 2, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16_UNORM, 2, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, 2, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16_UNORM, 2, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16_UINT, 2, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16_SNORM, 2, 2,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SINT, 2, 2,
                  DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_USCALED, VK_FORMAT_R16G16_USCALED, 2, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16G16_SSCALED, VK_FORMAT_R16G16_SSCALED, 2, 2,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_UINT, VK_FORMAT_R16_SFLOAT, 1, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_UINT, 1, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_UINT, VK_FORMAT_R16_UNORM, 1, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_SFLOAT, 1, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_UNORM, VK_FORMAT_R16_UNORM, 1, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_UINT, VK_FORMAT_R16_UINT, 1, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_SNORM, VK_FORMAT_R16_SNORM, 1, 2,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_SINT, VK_FORMAT_R16_SINT, 1, 2, DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_USCALED, VK_FORMAT_R16_USCALED, 1, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R16_SSCALED, VK_FORMAT_R16_SSCALED, 1, 2,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UNORM, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UINT, 4, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM, 4, 1,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_SINT, 4, 1,
                  DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_USCALED, VK_FORMAT_R8G8B8A8_USCALED, 4,
                  1, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8A8_SSCALED, VK_FORMAT_R8G8B8A8_SSCALED, 4,
                  1, DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_UINT, VK_FORMAT_R8G8B8_UNORM, 3, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8_UNORM, 3, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8_SRGB, 3, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_UINT, VK_FORMAT_R8G8B8_UINT, 3, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8_SNORM, 3, 1,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_SINT, VK_FORMAT_R8G8B8_SINT, 3, 1,
                  DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8_USCALED, 3, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8B8_SSCALED, VK_FORMAT_R8G8B8_SSCALED, 3, 1,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8_UNORM, 2, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_UNORM, 2, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_SRGB, VK_FORMAT_R8G8_SRGB, 2, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8_UINT, 2, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8_SNORM, 2, 1,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_SINT, VK_FORMAT_R8G8_SINT, 2, 1,
                  DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8_USCALED, 2, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8G8_SSCALED, VK_FORMAT_R8G8_SSCALED, 2, 1,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_UINT, VK_FORMAT_R8_UNORM, 1, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_UNORM, VK_FORMAT_R8_UNORM, 1, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_SRGB, VK_FORMAT_R8_SRGB, 1, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_UINT, VK_FORMAT_R8_UINT, 1, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_SNORM, VK_FORMAT_R8_SNORM, 1, 1, DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_SINT, VK_FORMAT_R8_SINT, 1, 1, DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_USCALED, VK_FORMAT_R8_USCALED, 1, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_R8_SSCALED, VK_FORMAT_R8_SSCALED, 1, 1,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_UINT, VK_FORMAT_B8G8R8A8_UNORM, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_UINT, VK_FORMAT_B8G8R8A8_SRGB, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, 4, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_SNORM, VK_FORMAT_B8G8R8A8_SNORM, 4, 1,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_UINT, VK_FORMAT_B8G8R8A8_UINT, 4, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_SINT, VK_FORMAT_B8G8R8A8_SINT, 4, 1,
                  DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_USCALED, VK_FORMAT_B8G8R8A8_USCALED, 4,
                  1, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_B8G8R8A8_SSCALED, VK_FORMAT_B8G8R8A8_SSCALED, 4,
                  1, DataType::UInt),

        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_UINT_PACK32,
                  VK_FORMAT_A8B8G8R8_UNORM_PACK32, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_UINT_PACK32,
                  VK_FORMAT_A8B8G8R8_SRGB_PACK32, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                  VK_FORMAT_A8B8G8R8_UNORM_PACK32, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_SRGB_PACK32,
                  VK_FORMAT_A8B8G8R8_SRGB_PACK32, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_SNORM_PACK32,
                  VK_FORMAT_A8B8G8R8_SNORM_PACK32, 4, 1, DataType::SNorm),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_UINT_PACK32,
                  VK_FORMAT_A8B8G8R8_UINT_PACK32, 4, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_SINT_PACK32,
                  VK_FORMAT_A8B8G8R8_SINT_PACK32, 4, 1, DataType::SInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_USCALED_PACK32,
                  VK_FORMAT_A8B8G8R8_USCALED_PACK32, 4, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
                  VK_FORMAT_A8B8G8R8_SSCALED_PACK32, 4, 1, DataType::UInt),

        TEST_CASE(TextureType::BC1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
                  0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
                  0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_UNORM_BLOCK, 0,
                  0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, VK_FORMAT_BC1_RGB_SRGB_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK, 0,
                  0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
                  0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 0,
                  0, DataType::UNorm),

        TEST_CASE(TextureType::BC2, VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC2, VK_FORMAT_BC2_SRGB_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC2, VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC2, VK_FORMAT_BC2_SRGB_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::BC3, VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC3, VK_FORMAT_BC3_SRGB_BLOCK, VK_FORMAT_BC3_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC3, VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC3, VK_FORMAT_BC3_SRGB_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::BC4, VK_FORMAT_BC4_SNORM_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC4, VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC4_SNORM_BLOCK, 0, 0,
                  DataType::SNorm),
        TEST_CASE(TextureType::BC4, VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC4, VK_FORMAT_BC4_SNORM_BLOCK, VK_FORMAT_BC4_SNORM_BLOCK, 0, 0,
                  DataType::SNorm),

        TEST_CASE(TextureType::BC5, VK_FORMAT_BC5_SNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC5, VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK, 0, 0,
                  DataType::SNorm),
        TEST_CASE(TextureType::BC5, VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC5, VK_FORMAT_BC5_SNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK, 0, 0,
                  DataType::SNorm),

        TEST_CASE(TextureType::BC6, VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_BC6H_UFLOAT_BLOCK, 0, 0,
                  DataType::Float),
        TEST_CASE(TextureType::BC6, VK_FORMAT_BC6H_UFLOAT_BLOCK, VK_FORMAT_BC6H_SFLOAT_BLOCK, 0, 0,
                  DataType::SNorm),
        TEST_CASE(TextureType::BC6, VK_FORMAT_BC6H_UFLOAT_BLOCK, VK_FORMAT_BC6H_UFLOAT_BLOCK, 0, 0,
                  DataType::Float),
        TEST_CASE(TextureType::BC6, VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_BC6H_SFLOAT_BLOCK, 0, 0,
                  DataType::SNorm),

        TEST_CASE(TextureType::BC7, VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_BC7_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC7, VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC7, VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_UNORM_BLOCK, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC7, VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::R9G9B9E5, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
                  VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 0, 0, DataType::Float),
        TEST_CASE(TextureType::G4R4, VK_FORMAT_R4G4_UNORM_PACK8, VK_FORMAT_R4G4_UNORM_PACK8, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::A4R4G4B4, VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                  VK_FORMAT_R4G4B4A4_UNORM_PACK16, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::A4R4G4B4, VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                  VK_FORMAT_B4G4R4A4_UNORM_PACK16, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::R5G6B5, VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16,
                  0, 0, DataType::UNorm),
        TEST_CASE(TextureType::R5G6B5, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_B5G6R5_UNORM_PACK16,
                  0, 0, DataType::UNorm),
        TEST_CASE(TextureType::A1R5G5B5, VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                  VK_FORMAT_R5G5B5A1_UNORM_PACK16, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::A1R5G5B5, VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                  VK_FORMAT_B5G5R5A1_UNORM_PACK16, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::R5G5B5A1, VK_FORMAT_A1R5G5B5_UNORM_PACK16,
                  VK_FORMAT_A1R5G5B5_UNORM_PACK16, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_UINT_PACK32,
                  VK_FORMAT_A2R10G10B10_UNORM_PACK32, 1, 4, DataType::UNorm),
        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                  VK_FORMAT_A2R10G10B10_UNORM_PACK32, 1, 4, DataType::UNorm),
        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                  VK_FORMAT_A2R10G10B10_SNORM_PACK32, 1, 4, DataType::SNorm),
        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_USCALED_PACK32,
                  VK_FORMAT_A2R10G10B10_USCALED_PACK32, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
                  VK_FORMAT_A2R10G10B10_SSCALED_PACK32, 1, 4, DataType::SInt),
        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_UINT_PACK32,
                  VK_FORMAT_A2R10G10B10_UINT_PACK32, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::RGB10A2, VK_FORMAT_A2R10G10B10_SINT_PACK32,
                  VK_FORMAT_A2R10G10B10_SINT_PACK32, 1, 4, DataType::SInt),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_UINT_PACK32,
                  VK_FORMAT_A2B10G10R10_UNORM_PACK32, 1, 4, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                  VK_FORMAT_A2B10G10R10_UNORM_PACK32, 1, 4, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                  VK_FORMAT_A2B10G10R10_SNORM_PACK32, 1, 4, DataType::SNorm),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_USCALED_PACK32,
                  VK_FORMAT_A2B10G10R10_USCALED_PACK32, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
                  VK_FORMAT_A2B10G10R10_SSCALED_PACK32, 1, 4, DataType::SInt),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_UINT_PACK32,
                  VK_FORMAT_A2B10G10R10_UINT_PACK32, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_A2B10G10R10_SINT_PACK32,
                  VK_FORMAT_A2B10G10R10_SINT_PACK32, 1, 4, DataType::SInt),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                  VK_FORMAT_B10G11R11_UFLOAT_PACK32, 0, 0, DataType::Float),
    };

    for(VKFormat f : color_tests)
      AddSupportedTests(f, test_textures, false);

    // finally add the depth tests
    const VKFormat depth_tests[] = {
        TEST_CASE(TextureType::Unknown, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                  0, 0, DataType::Float),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                  0, 0, DataType::UInt),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, 0,
                  0, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, 0,
                  0, DataType::UInt),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, 0,
                  0, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, 0,
                  0, DataType::UInt),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT, 0, 0,
                  DataType::Float),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_X8_D24_UNORM_PACK32,
                  VK_FORMAT_X8_D24_UNORM_PACK32, 0, 0, DataType::Float),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM, 0, 0,
                  DataType::Float),

        TEST_CASE(TextureType::Unknown, VK_FORMAT_S8_UINT, VK_FORMAT_S8_UINT, 0, 0, DataType::UInt),
    };

    for(VKFormat f : depth_tests)
      AddSupportedTests(f, test_textures, true);

    uploadBuf.unmap();

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    pipeCreateInfo.layout = layout;

    pipeCreateInfo.stages = {
        vs,
        vs,
    };

    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;

    renderPassCreateInfo.attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    renderPassCreateInfo.attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkPipelineShaderStageCreateInfo msps[(size_t)DataType::Count];

    std::map<std::string, std::string> macros;
    macros["TEX_WIDTH"] = std::to_string(texWidth);

    msps[(size_t)DataType::Float] = msps[(size_t)DataType::UNorm] = msps[(size_t)DataType::SNorm] =
        CompileShaderModule(pixelMSFloat, ShaderLang::glsl, ShaderStage::frag, "main", macros);
    msps[(size_t)DataType::UInt] =
        CompileShaderModule(pixelMSUInt, ShaderLang::glsl, ShaderStage::frag, "main", macros);
    msps[(size_t)DataType::SInt] =
        CompileShaderModule(pixelMSSInt, ShaderLang::glsl, ShaderStage::frag, "main", macros);

    VkPipelineShaderStageCreateInfo msdepthps =
        CompileShaderModule(pixelMSDepth, ShaderLang::glsl, ShaderStage::frag, "main", macros);

    for(TestCase &t : test_textures)
    {
      if(t.res.image == VK_NULL_HANDLE || t.hasData)
        continue;

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

      if(t.isDepth)
      {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(t.fmt.viewFmt == VK_FORMAT_S8_UINT)
          aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        else if(t.fmt.viewFmt == VK_FORMAT_D16_UNORM_S8_UINT ||
                t.fmt.viewFmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                t.fmt.viewFmt == VK_FORMAT_D32_SFLOAT_S8_UINT)
          aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(
                       0,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       t.res.image, vkh::ImageSubresourceRange(aspectMask)),
               });

      if(!t.canRender && !t.isDepth)
      {
        TEST_WARN("Need data for test %s %s, but it's not a renderable/depthable format",
                  t.fmt.name.c_str(), MakeName(t).c_str());

        vkEndCommandBuffer(cmd);

        Submit(99, 99, {cmd});
        CHECK_VKR(vkDeviceWaitIdle(device));

        continue;
      }

      pipeCreateInfo.depthStencilState.depthTestEnable = t.isDepth;
      pipeCreateInfo.depthStencilState.depthWriteEnable = t.isDepth;
      pipeCreateInfo.depthStencilState.stencilTestEnable = t.isDepth;

      pipeCreateInfo.multisampleState.sampleShadingEnable = t.isMSAA;
      pipeCreateInfo.multisampleState.minSampleShading = t.isMSAA ? 1.0f : 0.0f;

      bool tex3d = t.dim == 3;

      uint32_t mipLevels = texMips, sampleCount = 1;

      if(t.isMSAA)
      {
        mipLevels = 1;
        sampleCount = texSamples;
      }

      pushMarker(cmd, "Render data for " + t.fmt.name + " " + MakeName(t));

      t.hasData = true;

      bool srgb = false, bgra = false;
      switch(t.fmt.viewFmt)
      {
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: bgra = true; break;

        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
          bgra = true;
          srgb = true;
          break;

        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: srgb = true; break;
        default: break;
      }

      int flags = 0;

      if(t.fmt.cfg.data == DataType::SNorm)
        flags |= 1;
      if(srgb)
        flags |= 2;
      if(bgra)
        flags |= 4;

      renderPassCreateInfo.attachments[0].format = t.fmt.viewFmt;

      VkAttachmentReference *attRef =
          (VkAttachmentReference *)renderPassCreateInfo.subpasses[0].pColorAttachments;

      if(t.isDepth)
      {
        pipeCreateInfo.stages[1] = msdepthps;

        renderPassCreateInfo.dependencies[0].srcStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        renderPassCreateInfo.dependencies[0].srcAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        renderPassCreateInfo.subpasses[0].colorAttachmentCount = 0;
        renderPassCreateInfo.subpasses[0].pDepthStencilAttachment =
            renderPassCreateInfo.subpasses[0].pColorAttachments;

        attRef->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        pipeCreateInfo.dynamicState.dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        };
      }
      else
      {
        pipeCreateInfo.stages[1] = msps[(size_t)t.fmt.cfg.data];

        renderPassCreateInfo.dependencies[0].srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        renderPassCreateInfo.dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderPassCreateInfo.subpasses[0].colorAttachmentCount = 1;
        renderPassCreateInfo.subpasses[0].pDepthStencilAttachment = NULL;

        attRef->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        pipeCreateInfo.dynamicState.dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
      }

      pipeCreateInfo.multisampleState.rasterizationSamples = VkSampleCountFlagBits(sampleCount);
      renderPassCreateInfo.attachments[0].samples = VkSampleCountFlagBits(sampleCount);

      VkRenderPass tempRP;
      CHECK_VKR(vkCreateRenderPass(device, renderPassCreateInfo, NULL, &tempRP));

      pipeCreateInfo.renderPass = tempRP;

      VkViewport view = {0.0f, 0.0f, texWidth, texHeight, 0.0f, 1.0f};
      VkRect2D scissor = {{}, {texWidth, texHeight}};
      vkCmdSetViewport(cmd, 0, 1, &view);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      // keep all PSOs/Framebuffers alive until the end of the loop where we submit the command
      // buffer
      std::vector<VkPipeline> tempPipes;
      std::vector<VkImageView> tempViews;
      std::vector<VkFramebuffer> tempFBs;

      for(uint32_t mp = 0; mp < mipLevels; mp++)
      {
        pushMarker(cmd, "Mip " + std::to_string(mp));

        uint32_t numSlices = 1;
        if(tex3d)
          numSlices = std::max(1U, texDepth >> mp);
        else if(t.isArray)
          numSlices = texSlices;

        scissor.extent.width = std::max(1U, texWidth >> mp);
        scissor.extent.height = std::max(1U, texHeight >> mp);

        if(t.dim == 1)
          scissor.extent.height = 1;

        for(uint32_t sl = 0; sl < numSlices; sl++)
        {
          pushMarker(cmd, "Slice " + std::to_string(sl));

          VkImageView tempView;
          VkFramebuffer tempFB;

          VkImageViewType viewType = t.viewType;
          if(viewType == VK_IMAGE_VIEW_TYPE_3D)
            viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

          vkCreateImageView(
              device,
              vkh::ImageViewCreateInfo(t.res.image, viewType, t.fmt.viewFmt, {},
                                       vkh::ImageSubresourceRange(aspectMask, mp, 1, sl, 1)),
              NULL, &tempView);
          vkCreateFramebuffer(
              device, vkh::FramebufferCreateInfo(tempRP, {tempView}, scissor.extent), NULL, &tempFB);

          tempViews.push_back(tempView);
          tempFBs.push_back(tempFB);

          if(t.isDepth)
          {
            vkCmdBeginRenderPass(
                cmd, vkh::RenderPassBeginInfo(tempRP, tempFB, scissor, {vkh::ClearValue(0.0f, 0)}),
                VK_SUBPASS_CONTENTS_INLINE);

            VkSampleMask sampleMask = 1;
            pipeCreateInfo.multisampleState.pSampleMask = &sampleMask;

            // need to do each sample separately to let us vary the stencil value
            for(uint32_t sm = 0; sm < sampleCount; sm++)
            {
              if(sampleCount > 1)
                pushMarker(cmd, "Sample " + std::to_string(sm));

              sampleMask = 1 << sm;

              VkPipeline pipe;
              vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipeCreateInfo, NULL, &pipe);
              tempPipes.push_back(pipe);

              vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

              Vec4i params(tex3d ? 0 : sl, mp, 0, tex3d ? sl : 0);
              vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, sizeof(params), &params);

              vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 100 + (mp + sm) * 10);

              setMarker(cmd, "Render depth, and first stencil");

              vkCmdDraw(cmd, 4, 1, 0, 0);

              // clip off the diagonal
              params.z = 1;
              vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, sizeof(params), &params);

              vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 10 + (mp + sm) * 10);

              setMarker(cmd, "Second stencil pass (with discard)");

              vkCmdDraw(cmd, 4, 1, 0, 0);

              if(sampleCount > 1)
                popMarker(cmd);
            }

            vkCmdEndRenderPass(cmd);
          }
          else
          {
            vkCmdBeginRenderPass(
                cmd, vkh::RenderPassBeginInfo(tempRP, tempFB, scissor, {vkh::ClearValue()}),
                VK_SUBPASS_CONTENTS_INLINE);

            VkPipeline pipe;
            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipeCreateInfo, NULL, &pipe);
            tempPipes.push_back(pipe);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

            Vec4i params(tex3d ? 0 : sl, mp, flags, tex3d ? sl : 0);
            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, sizeof(params), &params);

            setMarker(cmd, "Colour render");

            vkCmdDraw(cmd, 4, 1, 0, 0);

            vkCmdEndRenderPass(cmd);
          }

          popMarker(cmd);
        }

        popMarker(cmd);
      }

      popMarker(cmd);

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
      CHECK_VKR(vkDeviceWaitIdle(device));

      vkDestroyRenderPass(device, tempRP, NULL);
      for(VkFramebuffer fb : tempFBs)
        vkDestroyFramebuffer(device, fb, NULL);
      for(VkImageView v : tempViews)
        vkDestroyImageView(device, v, NULL);
      for(VkPipeline pipe : tempPipes)
        vkDestroyPipeline(device, pipe, NULL);
    }

    std::vector<Vec4f> blue;
    blue.resize(64 * 64 * 64, Vec4f(0.0f, 0.0f, 1.0f, 1.0f));

    std::vector<Vec4f> green;
    green.resize(64 * 64, Vec4f(0.0f, 1.0f, 0.0f, 1.0f));

    CurBuf = uploadBuf.map();

    memcpy(CurBuf, blue.data(), blue.size() * sizeof(Vec4f));
    memcpy(CurBuf + blue.size() * sizeof(Vec4f), green.data(), green.size() * sizeof(Vec4f));

    uploadBuf.unmap();

    // slice testing textures

    TestCase slice_test_array = {};
    TestCase slice_test_3d = {};
    slice_test_array.dim = 2;
    slice_test_array.isArray = true;
    slice_test_array.res = AllocatedImage(
        this,
        vkh::ImageCreateInfo(64, 64, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 2, 64),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    slice_test_array.view = createImageView(vkh::ImageViewCreateInfo(
        slice_test_array.res.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R32G32B32A32_SFLOAT));
    slice_test_array.set = allocateDescriptorSet(setlayout);
    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(slice_test_array.set, 0,
                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            {vkh::DescriptorImageInfo(slice_test_array.view)}),
                });

    slice_test_3d.dim = 3;
    slice_test_3d.res = AllocatedImage(
        this,
        vkh::ImageCreateInfo(64, 64, 64, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 2),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    slice_test_3d.view = createImageView(vkh::ImageViewCreateInfo(
        slice_test_3d.res.image, VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R32G32B32A32_SFLOAT));
    slice_test_3d.set = allocateDescriptorSet(setlayout);
    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(slice_test_3d.set, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    {vkh::DescriptorImageInfo(slice_test_3d.view)}),
        });

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      slice_test_array.res.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, slice_test_3d.res.image),
          });

      VkBufferImageCopy copy = {};
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageExtent = {64, 64, 1};
      copy.imageSubresource.layerCount = 64;

      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_array.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      copy.imageExtent.depth = 64;
      copy.imageSubresource.layerCount = 1;
      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_3d.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      copy.imageSubresource.mipLevel = 1;
      copy.imageExtent = {32, 32, 1};
      copy.imageSubresource.layerCount = 64;
      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_array.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      copy.imageExtent.depth = 32;
      copy.imageSubresource.layerCount = 1;
      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_3d.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      slice_test_array.res.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, slice_test_3d.res.image),
          });

      copy.imageSubresource.mipLevel = 0;
      copy.imageSubresource.baseArrayLayer = 17;
      copy.imageExtent = {64, 64, 1};
      copy.imageSubresource.layerCount = 1;
      copy.bufferOffset = blue.size() * sizeof(Vec4f);

      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_array.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      copy.imageSubresource.baseArrayLayer = 0;
      copy.imageOffset.z = 17;
      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_3d.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      copy.imageSubresource.mipLevel = 1;
      copy.imageExtent = {32, 32, 1};
      copy.imageOffset.z = 0;
      copy.imageSubresource.baseArrayLayer = 17;
      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_array.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      copy.imageSubresource.baseArrayLayer = 0;
      copy.imageOffset.z = 17;
      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, slice_test_3d.res.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                           slice_test_array.res.image),
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                           slice_test_3d.res.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});

      vkDeviceWaitIdle(device);
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(rp, framebuffer, mainWindow->scissor,
                                                    {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      VkViewport view = {0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f};

      {
        VkRect2D scissor = {
            {(int32_t)view.x + 1, (int32_t)view.y + 1},
            {(uint32_t)view.width - 2, (uint32_t)view.height - 2},
        };
        vkCmdSetViewport(cmd, 0, 1, &view);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
      }

      // dummy draw for each slice test texture
      pushMarker(cmd, "slice tests");
      setMarker(cmd, "2D array");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPSO(slice_test_array));
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                                 {slice_test_array.set}, {});
      vkCmdDraw(cmd, 0, 0, 0, 0);

      setMarker(cmd, "3D");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPSO(slice_test_3d));
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                                 {slice_test_3d.set}, {});
      vkCmdDraw(cmd, 0, 0, 0, 0);
      popMarker(cmd);

      for(size_t i = 0; i < test_textures.size(); i++)
      {
        if(i == 0 || test_textures[i].fmt.texFmt != test_textures[i - 1].fmt.texFmt ||
           test_textures[i].fmt.viewFmt != test_textures[i - 1].fmt.viewFmt ||
           test_textures[i].fmt.cfg.data != test_textures[i - 1].fmt.cfg.data)
        {
          if(i != 0)
            popMarker(cmd);

          pushMarker(cmd, test_textures[i].fmt.name);
        }

        setMarker(cmd, MakeName(test_textures[i]));

        VkRect2D scissor = {
            {(int32_t)view.x + 1, (int32_t)view.y + 1},
            {(uint32_t)view.width - 2, (uint32_t)view.height - 2},
        };
        vkCmdSetViewport(cmd, 0, 1, &view);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPSO(test_textures[i]));

        if(test_textures[i].set != VK_NULL_HANDLE)
        {
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                                     {test_textures[i].set}, {});
          vkCmdDraw(cmd, 4, 1, 0, 0);
        }
        else
        {
          setMarker(cmd, "UNSUPPORTED");
        }

        // advance to next viewport
        view.x += view.width;
        if(view.x + view.width > (float)screenWidth)
        {
          view.x = 0;
          view.y += view.height;
        }
      }

      // pop the last format region
      popMarker(cmd);

      vkCmdEndRenderPass(cmd);

      blitToSwap(cmd, fltTex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapimg,
                 VK_IMAGE_LAYOUT_GENERAL);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
