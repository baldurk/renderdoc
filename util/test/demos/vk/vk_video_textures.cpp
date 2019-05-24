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

#include "vk_test.h"

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
//                          **** WARNING ****                                    //
//                                                                               //
// When comparing to D3D tests, the order of channels in the data is *not*       //
// necessarily the same - vulkan expects Y in G, Cb/U in B and Cr/V in R         //
// consistently, where some of the D3D formats are a bit different.              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

TEST(VK_Video_Textures, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests of YUV textures";

  std::string common = R"EOSHADER(

#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

)EOSHADER";

  const std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

#define MODE_RGB 0
#define MODE_YUV_DEFAULT 1

layout(set = 0, binding = 0, std140) uniform constsbuf
{
  ivec2 dimensions;
  ivec2 downsampling;
  int y_channel;
  int u_channel;
  int v_channel;
  int mode;
};

layout(set = 0, binding = 1) uniform texture2D tex;
layout(set = 0, binding = 2) uniform texture2D tex2;
layout(set = 0, binding = 3) uniform texture2D tex3;

void main()
{
  ivec2 coord = ivec2(vertIn.uv.xy * vec2(dimensions.xy));

  bool odd = false;

	vec4 texvec = texelFetch(tex, coord, 0);

  // detect interleaved 4:2:2.
  // 4:2:0 will have downsampling.x == downsampling.y == 2,
  // 4:4:4 will have downsampling.x == downsampling.y == 1
  // planar formats will have one one channel >= 4 i.e. in the second texture.
  if(downsampling.x > downsampling.y && y_channel < 4 && u_channel < 4 && v_channel < 4)
  {
    // texels come out as just RG for some reason, so we need to fetch the adjacent texel to
    // get the other half of the uv data, the y sample is left as-is
    if((coord.x & 1) != 0)
    {
      coord.x &= ~1;
      texvec.b = texelFetch(tex, coord, 0).g;
    }
    else
    {
      coord.x |= 1;
      texvec.b = texvec.g;
      texvec.g = texelFetch(tex, coord, 0).g;
    }
  }

  if(mode == MODE_RGB) { Color = texvec; return; }

  coord = ivec2(vertIn.uv.xy * vec2(dimensions.xy) / vec2(downsampling.xy));

	vec4 texvec2 = texelFetch(tex2, coord, 0);
	vec4 texvec3 = texelFetch(tex3, coord, 0);

  float texdata[] = {
    texvec.x,  texvec.y,  texvec.z,  texvec.w,
    texvec2.x, texvec2.y, texvec2.z, texvec2.w,
    texvec3.x, texvec3.y, texvec3.z, texvec3.w,
  };

  float Y = texdata[y_channel];
  float U = texdata[u_channel];
  float V = texdata[v_channel];
  float A = float(texvec.w);

  const float Kr = 0.2126f;
  const float Kb = 0.0722f;

  float L = Y;
  float Pb = U - 0.5f;
  float Pr = V - 0.5f;

  // these are just reversals of the equations below

  float B = L + (Pb / 0.5f) * (1 - Kb);
  float R = L + (Pr / 0.5f) * (1 - Kr);
  float G = (L - Kr * R - Kb * B) / (1.0f - Kr - Kb);

  Color = vec4(R, G, B, A);
}

)EOSHADER";

  const std::string pixel_sampled = R"EOSHADER(
layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main()
{
  Color = texture(tex, vertIn.uv.xy);
}

)EOSHADER";

  struct YUVPixel
  {
    uint16_t Y, Cb, Cr, A;
  };

  // we use a plain un-scaled un-offsetted direct conversion
  YUVPixel RGB2YUV(uint32_t rgba)
  {
    uint32_t r = rgba & 0xff;
    uint32_t g = (rgba >> 8) & 0xff;
    uint32_t b = (rgba >> 16) & 0xff;
    uint16_t a = (rgba >> 24) & 0xff;

    const float Kr = 0.2126f;
    const float Kb = 0.0722f;

    float R = float(r) / 255.0f;
    float G = float(g) / 255.0f;
    float B = float(b) / 255.0f;

    // calculate as floats since we're not concerned with performance here
    float L = Kr * R + Kb * B + (1.0f - Kr - Kb) * G;

    float Pb = ((B - L) / (1 - Kb)) * 0.5f;
    float Pr = ((R - L) / (1 - Kr)) * 0.5f;
    float fA = float(a) / 255.0f;

    uint16_t Y = (uint16_t)(L * 65536.0f);
    uint16_t Cb = (uint16_t)((Pb + 0.5f) * 65536.0f);
    uint16_t Cr = (uint16_t)((Pr + 0.5f) * 65536.0f);
    uint16_t A = (uint16_t)(fA * 65535.0f);

    return {Y, Cb, Cr, A};
  }

  struct TextureData
  {
    AllocatedImage tex;
    const char *name = NULL;
    VkImageView views[3] = {};
    AllocatedBuffer cb;
    VkDescriptorSet descset;
  };

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);

    // add required dependency extensions
    devExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    const DefaultA2V verts[4] = {
        {Vec3f(-1.0f, -1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, -1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f)},
        {Vec3f(1.0f, 1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(allocator,
                       vkh::BufferCreateInfo(sizeof(verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(verts);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    std::vector<byte> yuv8;
    std::vector<uint16_t> yuv16;
    yuv8.reserve(rgba8.data.size() * 4);
    yuv16.reserve(rgba8.data.size() * 4);

    for(uint32_t y = 0; y < rgba8.height; y++)
    {
      for(uint32_t x = 0; x < rgba8.width; x++)
      {
        YUVPixel p = RGB2YUV(rgba8.data[y * rgba8.width + x]);

        yuv16.push_back(p.Cr);
        yuv16.push_back(p.Y);
        yuv16.push_back(p.Cb);
        yuv16.push_back(p.A);

        yuv8.push_back(p.Cr >> 8);
        yuv8.push_back(p.Y >> 8);
        yuv8.push_back(p.Cb >> 8);
        yuv8.push_back(p.A >> 8);
      }
    }

    VkFormatFeatureFlagBits reqsupp = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    TextureData textures[20] = {};
    uint32_t texidx = 0;

    AllocatedBuffer uploadBuf(allocator, vkh::BufferCreateInfo(rgba8.width * rgba8.height * 16,
                                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                              VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    auto make_tex = [&](const char *name, uint32_t subsampling, VkFormat texFmt, VkFormat viewFmt,
                        VkFormat view2Fmt, VkFormat view3Fmt, Vec4i config, void *data, size_t sz,
                        uint32_t rowPitch) {
      VkFormatProperties props = {};
      vkGetPhysicalDeviceFormatProperties(phys, texFmt, &props);

      {
        TEST_LOG("%s supports:", name);
        if(props.optimalTilingFeatures == 0)
          TEST_LOG("  - NONE");
#define CHECK_SUPP(s)                                     \
  if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_##s) \
    TEST_LOG("  - " #s);
        CHECK_SUPP(SAMPLED_IMAGE_BIT)
        CHECK_SUPP(STORAGE_IMAGE_BIT)
        CHECK_SUPP(STORAGE_IMAGE_ATOMIC_BIT)
        CHECK_SUPP(UNIFORM_TEXEL_BUFFER_BIT)
        CHECK_SUPP(STORAGE_TEXEL_BUFFER_BIT)
        CHECK_SUPP(STORAGE_TEXEL_BUFFER_ATOMIC_BIT)
        CHECK_SUPP(VERTEX_BUFFER_BIT)
        CHECK_SUPP(COLOR_ATTACHMENT_BIT)
        CHECK_SUPP(COLOR_ATTACHMENT_BLEND_BIT)
        CHECK_SUPP(DEPTH_STENCIL_ATTACHMENT_BIT)
        CHECK_SUPP(BLIT_SRC_BIT)
        CHECK_SUPP(BLIT_DST_BIT)
        CHECK_SUPP(SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        CHECK_SUPP(TRANSFER_SRC_BIT)
        CHECK_SUPP(TRANSFER_DST_BIT)
        CHECK_SUPP(MIDPOINT_CHROMA_SAMPLES_BIT)
        CHECK_SUPP(SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT)
        CHECK_SUPP(SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT)
        CHECK_SUPP(SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT)
        CHECK_SUPP(SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT)
        CHECK_SUPP(DISJOINT_BIT)
        CHECK_SUPP(COSITED_CHROMA_SAMPLES_BIT)
        CHECK_SUPP(SAMPLED_IMAGE_FILTER_CUBIC_BIT_IMG)
        CHECK_SUPP(SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT)
      }

      uint32_t horizDownsampleFactor = ((subsampling % 100) / 10);
      uint32_t vertDownsampleFactor = (subsampling % 10);

      // 4:4:4
      if(horizDownsampleFactor == 4 && vertDownsampleFactor == 4)
      {
        horizDownsampleFactor = vertDownsampleFactor = 1;
      }

      // 4:2:2
      else if(horizDownsampleFactor == 2 && vertDownsampleFactor == 2)
      {
        vertDownsampleFactor = 1;
      }

      // 4:2:0
      else if(horizDownsampleFactor == 2 && vertDownsampleFactor == 0)
      {
        vertDownsampleFactor = 2;
      }
      else
      {
        TEST_FATAL("Unhandled subsampling %d", subsampling);
      }

      if(VkFormatFeatureFlagBits(props.optimalTilingFeatures & reqsupp) == reqsupp)
      {
        TextureData &t = textures[texidx];
        t.name = name;

        t.tex.create(allocator, vkh::ImageCreateInfo(
                                    rgba8.width, rgba8.height, 0, texFmt,
                                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 1,
                                    1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT),
                     VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
        Vec4i cbdata[2] = {
            Vec4i(rgba8.width, rgba8.height, horizDownsampleFactor, vertDownsampleFactor), config,
        };

        t.cb.create(allocator,
                    vkh::BufferCreateInfo(sizeof(cbdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                    VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

        t.cb.upload(cbdata);

        uploadBuf.upload(data, sz);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        vkh::cmdPipelineBarrier(
            cmd,
            {
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, t.tex.image),
            });

        std::vector<VkBufferImageCopy> regions;

        if(view3Fmt != VK_FORMAT_UNDEFINED)
        {
          VkBufferImageCopy copy = {};
          copy.bufferOffset = rowPitch * rgba8.height * 2;
          copy.bufferRowLength = 0;
          copy.bufferImageHeight = 0;

          copy.imageExtent.width = rgba8.width / horizDownsampleFactor;
          copy.imageExtent.height = rgba8.height / vertDownsampleFactor;
          copy.imageExtent.depth = 1;
          copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_2_BIT;
          copy.imageSubresource.layerCount = 1;
          regions.push_back(copy);
        }
        if(view2Fmt != VK_FORMAT_UNDEFINED)
        {
          VkBufferImageCopy copy = {};
          copy.bufferOffset = rowPitch * rgba8.height;
          copy.bufferRowLength = 0;
          copy.bufferImageHeight = 0;

          copy.imageExtent.width = rgba8.width / horizDownsampleFactor;
          copy.imageExtent.height = rgba8.height / vertDownsampleFactor;
          copy.imageExtent.depth = 1;
          copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
          copy.imageSubresource.layerCount = 1;
          regions.push_back(copy);
        }

        {
          VkBufferImageCopy copy = {};
          copy.bufferOffset = 0;
          copy.bufferRowLength = 0;
          copy.bufferImageHeight = 0;

          copy.imageExtent.width = rgba8.width;
          copy.imageExtent.height = rgba8.height;
          copy.imageExtent.depth = 1;
          copy.imageSubresource.aspectMask = view2Fmt != VK_FORMAT_UNDEFINED
                                                 ? VK_IMAGE_ASPECT_PLANE_0_BIT
                                                 : VK_IMAGE_ASPECT_COLOR_BIT;
          copy.imageSubresource.layerCount = 1;
          regions.push_back(copy);
        }
        vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, t.tex.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)regions.size(),
                               regions.data());

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, t.tex.image),
                 });

        vkEndCommandBuffer(cmd);

        Submit(99, 99, {cmd});
        vkDeviceWaitIdle(device);

        t.descset = allocateDescriptorSet(setlayout);

        vkh::updateDescriptorSets(
            device, {
                        vkh::WriteDescriptorSet(t.descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                {vkh::DescriptorBufferInfo(t.cb.buffer)}),
                    });

        if(view3Fmt != VK_FORMAT_UNDEFINED)
        {
          t.views[0] = createImageView(
              vkh::ImageViewCreateInfo(t.tex.image, VK_IMAGE_VIEW_TYPE_2D, viewFmt, {},
                                       vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_PLANE_0_BIT)));
          t.views[1] = createImageView(
              vkh::ImageViewCreateInfo(t.tex.image, VK_IMAGE_VIEW_TYPE_2D, view2Fmt, {},
                                       vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_PLANE_1_BIT)));
          t.views[2] = createImageView(
              vkh::ImageViewCreateInfo(t.tex.image, VK_IMAGE_VIEW_TYPE_2D, view3Fmt, {},
                                       vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_PLANE_2_BIT)));

          vkh::updateDescriptorSets(
              device, {
                          vkh::WriteDescriptorSet(t.descset, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[0])}),
                          vkh::WriteDescriptorSet(t.descset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[1])}),
                          vkh::WriteDescriptorSet(t.descset, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[2])}),
                      });
        }
        else if(view2Fmt != VK_FORMAT_UNDEFINED)
        {
          t.views[0] = createImageView(
              vkh::ImageViewCreateInfo(t.tex.image, VK_IMAGE_VIEW_TYPE_2D, viewFmt, {},
                                       vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_PLANE_0_BIT)));
          t.views[1] = createImageView(
              vkh::ImageViewCreateInfo(t.tex.image, VK_IMAGE_VIEW_TYPE_2D, view2Fmt, {},
                                       vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_PLANE_1_BIT)));

          vkh::updateDescriptorSets(
              device, {
                          vkh::WriteDescriptorSet(t.descset, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[0])}),
                          vkh::WriteDescriptorSet(t.descset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[1])}),
                          vkh::WriteDescriptorSet(t.descset, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[1])}),
                      });
        }
        else
        {
          t.views[0] = createImageView(
              vkh::ImageViewCreateInfo(t.tex.image, VK_IMAGE_VIEW_TYPE_2D, viewFmt, {},
                                       vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)));

          vkh::updateDescriptorSets(
              device, {
                          vkh::WriteDescriptorSet(t.descset, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[0])}),
                          vkh::WriteDescriptorSet(t.descset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[0])}),
                          vkh::WriteDescriptorSet(t.descset, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                  {vkh::DescriptorImageInfo(t.views[0])}),
                      });
        }
      }
      texidx++;
    };

#define MAKE_TEX(sampling, texFmt, viewFmt, config, data_vector, stride)                         \
  make_tex(#texFmt, sampling, texFmt, viewFmt, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, config, \
           data_vector.data(), data_vector.size() * sizeof(data_vector[0]), stride);
#define MAKE_TEX2(sampling, texFmt, viewFmt, view2Fmt, config, data_vector, stride)   \
  make_tex(#texFmt, sampling, texFmt, viewFmt, view2Fmt, VK_FORMAT_UNDEFINED, config, \
           data_vector.data(), data_vector.size() * sizeof(data_vector[0]), stride);
#define MAKE_TEX3(sampling, texFmt, viewFmt, view2Fmt, view3Fmt, config, data_vector, stride)  \
  make_tex(#texFmt, sampling, texFmt, viewFmt, view2Fmt, view3Fmt, config, data_vector.data(), \
           data_vector.size() * sizeof(data_vector[0]), stride);

    MAKE_TEX(444, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, Vec4i(0, 0, 0, 0), rgba8.data,
             rgba8.width * 4);

    TEST_ASSERT(textures[0].descset != VK_NULL_HANDLE, "Expect RGBA8 to always work");

    // vulkan doesn't have 4:4:4 packed formats, makes sense as it can use normal formats
    // MAKE_TEX(AYUV, VK_FORMAT_R8G8B8A8_UNORM, Vec4i(2, 1, 0, 1), yuv8, rgba8.width * 4);
    // MAKE_TEX(Y416, VK_FORMAT_R16G16B16A16_UNORM, Vec4i(1, 0, 2, 1), yuv16, rgba8.width * 8);
    MAKE_TEX(444, VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR,
             VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR, Vec4i(1, 2, 0, 1), yuv16,
             rgba8.width * 8);

    ///////////////////////////////////////
    // 4:4:4 3-plane
    ///////////////////////////////////////
    {
      std::vector<byte> triplane8;
      triplane8.resize(yuv8.size());

      const byte *in = yuv8.data();
      byte *out[3] = {
          triplane8.data(), triplane8.data() + rgba8.width * rgba8.height,
          triplane8.data() + rgba8.width * rgba8.height * 2,
      };

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
      {
        *(out[0]++) = in[1];
        *(out[1]++) = in[2];
        *(out[2]++) = in[0];

        in += 4;
      }

      // we can re-use the same data for Y010 and Y016 as they share a format (with different bits)
      MAKE_TEX3(444, VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, VK_FORMAT_R8_UNORM, VK_FORMAT_R8_UNORM,
                VK_FORMAT_R8_UNORM, Vec4i(0, 4, 8, 1), triplane8, rgba8.width);
    }

    ///////////////////////////////////////
    // 4:2:2
    ///////////////////////////////////////
    {
      std::vector<byte> yuy2;
      yuy2.reserve(rgba8.data.size());

      const byte *in = yuv8.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i += 2)
      {
        // y0
        yuy2.push_back(in[1 + 0]);
        // avg(u0, u1)
        yuy2.push_back(byte((uint16_t(in[2 + 0]) + uint16_t(in[2 + 4])) >> 1));
        // y1
        yuy2.push_back(in[1 + 4]);
        // avg(v0, v1)
        yuy2.push_back(byte((uint16_t(in[0 + 0]) + uint16_t(in[0 + 4])) >> 1));

        in += 8;
      }

      MAKE_TEX(422, VK_FORMAT_G8B8G8R8_422_UNORM, VK_FORMAT_G8B8G8R8_422_UNORM, Vec4i(0, 2, 1, 1),
               yuy2, rgba8.width * 2);
    }

    {
      std::vector<byte> p208;
      p208.reserve(rgba8.data.size());

      const byte *in = yuv8.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
      {
        p208.push_back(in[1]);
        in += 4;
      }

      in = yuv8.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i += 2)
      {
        // avg(u0, u1)
        p208.push_back(byte((uint16_t(in[2 + 0]) + uint16_t(in[2 + 4])) >> 1));
        // avg(v0, v1)
        p208.push_back(byte((uint16_t(in[0 + 0]) + uint16_t(in[0 + 4])) >> 1));
        in += 8;
      }

      MAKE_TEX2(422, VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM,
                Vec4i(0, 4, 5, 1), p208, rgba8.width);
    }

    {
      std::vector<uint16_t> y216;
      y216.reserve(yuv16.size());

      const uint16_t *in = yuv16.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i += 2)
      {
        // y0
        y216.push_back(in[1 + 0]);
        // avg(u0, u1)
        y216.push_back(uint16_t((uint32_t(in[2 + 0]) + uint32_t(in[2 + 4])) >> 1));
        // y1
        y216.push_back(in[1 + 4]);
        // avg(v0, v1)
        y216.push_back(uint16_t((uint32_t(in[0 + 0]) + uint32_t(in[0 + 4])) >> 1));

        in += 8;
      }

      // we can re-use the same data for Y010 and Y016 as they share a format (with different bits)
      MAKE_TEX(422, VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
               VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16, Vec4i(0, 2, 1, 1), y216,
               rgba8.width * 4);
      MAKE_TEX(422, VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
               VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16, Vec4i(0, 2, 1, 1), y216,
               rgba8.width * 4);
    }

    uint32_t nv12idx = texidx;

    {
      std::vector<byte> nv12;
      nv12.reserve(rgba8.data.size());

      {
        const byte *in = yuv8.data();

        // luma plane
        for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
        {
          const byte Y = in[1];
          in += 4;

          nv12.push_back(Y);
        }
      }

      for(uint32_t row = 0; row < rgba8.height - 1; row += 2)
      {
        const byte *in = yuv8.data() + rgba8.width * 4 * row;
        const byte *in2 = yuv8.data() + rgba8.width * 4 * (row + 1);

        for(uint32_t i = 0; i < rgba8.width; i += 2)
        {
          const uint16_t Ua = in[2 + 0];
          const uint16_t Ub = in[2 + 4];
          const uint16_t Uc = in2[2 + 0];
          const uint16_t Ud = in2[2 + 4];

          const uint16_t Va = in[0 + 0];
          const uint16_t Vb = in[0 + 4];
          const uint16_t Vc = in2[0 + 0];
          const uint16_t Vd = in2[0 + 4];

          // midpoint average sample
          uint16_t U = (Ua + Ub + Uc + Ud) >> 2;
          uint16_t V = (Va + Vb + Vc + Vd) >> 2;

          in += 8;
          in2 += 8;

          nv12.push_back(byte(U));
          nv12.push_back(byte(V));
        }
      }

      MAKE_TEX2(420, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM,
                Vec4i(0, 4, 5, 1), nv12, rgba8.width);
    }

    {
      std::vector<uint16_t> p016;
      p016.reserve(rgba8.data.size() * 2);

      {
        const uint16_t *in = yuv16.data();

        // luma plane
        for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
        {
          const uint16_t Y = in[1];
          in += 4;

          p016.push_back(Y);
        }
      }

      for(uint32_t row = 0; row < rgba8.height - 1; row += 2)
      {
        const uint16_t *in = yuv16.data() + rgba8.width * 4 * row;
        const uint16_t *in2 = yuv16.data() + rgba8.width * 4 * (row + 1);

        for(uint32_t i = 0; i < rgba8.width; i += 2)
        {
          const uint32_t Ua = in[2 + 0];
          const uint32_t Ub = in[2 + 4];
          const uint32_t Uc = in2[2 + 0];
          const uint32_t Ud = in2[2 + 4];

          const uint32_t Va = in[0 + 0];
          const uint32_t Vb = in[0 + 4];
          const uint32_t Vc = in2[0 + 0];
          const uint32_t Vd = in2[0 + 4];

          // midpoint average sample
          uint32_t U = (Ua + Ub + Uc + Ud) / 4;
          uint32_t V = (Va + Vb + Vc + Vd) / 4;

          in += 8;
          in2 += 8;

          p016.push_back(uint16_t(U & 0xffff));
          p016.push_back(uint16_t(V & 0xffff));
        }
      }

      // we can re-use the same data for P010 and P016 as they share a format (with different bits)
      MAKE_TEX2(420, VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
                VK_FORMAT_R10X6_UNORM_PACK16, VK_FORMAT_R10X6G10X6_UNORM_2PACK16, Vec4i(0, 4, 5, 1),
                p016, rgba8.width * 2);
      MAKE_TEX2(420, VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR,
                VK_FORMAT_R12X4_UNORM_PACK16, VK_FORMAT_R12X4G12X4_UNORM_2PACK16, Vec4i(0, 4, 5, 1),
                p016, rgba8.width * 2);
    }

    VkSamplerYcbcrConversionCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
    };

    // when supported, add pipelines for sampling with ycbcr conversion from NV12
    struct
    {
      const char *name = "";
      VkSamplerYcbcrConversion conv = VK_NULL_HANDLE;
      VkSampler sampler = VK_NULL_HANDLE;
      VkPipeline pipe = VK_NULL_HANDLE;
      VkPipelineLayout layout = VK_NULL_HANDLE;
      VkDescriptorSet descset = VK_NULL_HANDLE;
    } ycbcr[6];

    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
    VkPhysicalDeviceFeatures2 feats = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &ycbcrFeats};
    vkGetPhysicalDeviceFeatures2KHR(phys, &feats);

    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, &props);

    // only do this test if LINEAR_FILTER is supported and ycbcr conversion, and our source view
    if(ycbcrFeats.samplerYcbcrConversion && textures[nv12idx].views[0] != VK_NULL_HANDLE &&
       (props.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT))
    {
      createInfo.chromaFilter = VK_FILTER_LINEAR;
      createInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
      createInfo.xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;
      createInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;

      vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcr[0].conv);
      ycbcr[0].name = "YCbCr 2020 Full";

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;

      vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcr[1].conv);
      ycbcr[1].name = "YCbCr 601 Narrow";

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;

      vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcr[2].conv);
      ycbcr[2].name = "RGB Identity Narrow";

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;

      vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcr[3].conv);
      ycbcr[3].name = "RGB Identity Full";

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;

      vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcr[4].conv);
      ycbcr[4].name = "YCbCr Identity Narrow";

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;

      vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcr[5].conv);
      ycbcr[5].name = "YCbCr Identity Full";

      pipeCreateInfo.stages = {
          CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
          CompileShaderModule(common + pixel_sampled, ShaderLang::glsl, ShaderStage::frag, "main"),
      };

      for(size_t i = 0; i < ARRAY_COUNT(ycbcr); i++)
      {
        VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        VkSamplerYcbcrConversionInfo ycbcrChain = {VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};

        sampInfo.pNext = &ycbcrChain;

        sampInfo.magFilter = VK_FILTER_LINEAR;
        sampInfo.minFilter = VK_FILTER_LINEAR;

        ycbcrChain.conversion = ycbcr[i].conv;
        vkCreateSampler(device, &sampInfo, NULL, &ycbcr[i].sampler);

        setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
             &ycbcr[i].sampler},
        }));

        pipeCreateInfo.layout = ycbcr[i].layout =
            createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

        ycbcr[i].pipe = createGraphicsPipeline(pipeCreateInfo);

        ycbcr[i].descset = allocateDescriptorSet(setlayout);

        vkh::ImageViewCreateInfo viewCreateInfo(
            textures[nv12idx].tex.image, VK_IMAGE_VIEW_TYPE_2D, createInfo.format, {},
            vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));

        viewCreateInfo.pNext = &ycbcrChain;

        VkImageView view = createImageView(viewCreateInfo);

        vkh::updateDescriptorSets(
            device, {
                        vkh::WriteDescriptorSet(ycbcr[i].descset, 0,
                                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                {vkh::DescriptorImageInfo(view)}),
                    });
      }
    }

    // need two pipeline layouts and two new pipelines, since these must be immutable samplers

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      float x = 1.0f, y = 1.0f;
      float w = 48.0f, h = 48.0f;

      for(size_t i = 0; i < ARRAY_COUNT(textures); i++)
      {
        TextureData &tex = textures[i];

        if(tex.tex.image)
        {
          setMarker(cmd, tex.name);

          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &tex.descset,
                                  0, NULL);

          VkViewport v = {x, y, w, h, 0.0f, 1.0f};
          vkCmdSetViewport(cmd, 0, 1, &v);
          vkCmdDraw(cmd, 4, 1, 0, 0);
        }

        x += 50.0f;

        if(x + 1.0f >= (float)screenWidth)
        {
          x = 1.0f;
          y += 50.0f;
        }
      }

      x = 2.0f;
      y = 202.0f;
      w = h = 96.0f;

      for(size_t i = 0; i < ARRAY_COUNT(ycbcr); i++)
      {
        if(ycbcr[i].pipe != VK_NULL_HANDLE)
        {
          setMarker(cmd, ycbcr[i].name);

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ycbcr[i].pipe);
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ycbcr[i].layout, 0, 1,
                                  &ycbcr[i].descset, 0, NULL);

          VkViewport v = {x, y, w, h, 0.0f, 1.0f};
          vkCmdSetViewport(cmd, 0, 1, &v);
          vkCmdDraw(cmd, 4, 1, 0, 0);
        }

        x += 60.0f;
      }

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDeviceWaitIdle(device);

    for(size_t i = 0; i < ARRAY_COUNT(ycbcr); i++)
    {
      vkDestroySampler(device, ycbcr[i].sampler, NULL);

      vkDestroySamplerYcbcrConversionKHR(device, ycbcr[i].conv, NULL);
    }

    return 0;
  }
};

REGISTER_TEST();