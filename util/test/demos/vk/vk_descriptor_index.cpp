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

#define STRESS_TEST 0

#if STRESS_TEST

#define DESC_ARRAY1_SIZE 4096
#define DESC_ARRAY2_SIZE (1024 * 1024 - DESC_ARRAY1_SIZE)

#else

#define DESC_ARRAY1_SIZE 128
#define DESC_ARRAY2_SIZE (512)

#endif

#define DESC_ARRAY3_SIZE 3

#define BUFIDX 15
#define INDEX3 4
#define INDEX1 49
#define INDEX2 381
#define NONUNIFORMIDX 20
#define TEX3_INDEX 1
#define ALIAS1_INDEX 6
#define ALIAS2_INDEX 12

RD_TEST(VK_Descriptor_Indexing, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle using descriptor indexing with large descriptor sets.";

  std::string common = R"EOSHADER(

#version 450 core

#extension GL_EXT_nonuniform_qualifier : require

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

  const std::string comp = R"EOSHADER(

#version 450 core

#extension GL_EXT_nonuniform_qualifier : require

layout(push_constant) uniform PushData
{
  uint bufidx;
  uint idx1;
  uint idx2;
  uint idx3;
  uint idx4;
  uint idx5;
} push;

struct tex_ref
{
  uint binding;
  uint idx;
};

layout(binding = 0, std430) buffer outbuftype {
  tex_ref outrefs[];
} outbuf[];

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(constant_id = 1) const int spec_canary = 0;

void main()
{
  if(spec_canary != 1337) return;

  outbuf[push.bufidx].outrefs[0].binding = 0;
  outbuf[push.bufidx].outrefs[0].idx = push.idx1;
  outbuf[push.bufidx].outrefs[1].binding = 2;
  outbuf[push.bufidx].outrefs[1].idx = push.idx2;

  outbuf[push.bufidx].outrefs[2].binding = 1;
  outbuf[push.bufidx].outrefs[2].idx = push.idx1;
  outbuf[push.bufidx].outrefs[3].binding = 2;
  outbuf[push.bufidx].outrefs[3].idx = push.idx2+5;

  outbuf[push.bufidx].outrefs[4].binding = 3;
  outbuf[push.bufidx].outrefs[4].idx = push.idx3;

  outbuf[push.bufidx].outrefs[5].binding = 4;
  outbuf[push.bufidx].outrefs[5].idx = push.idx4;

  outbuf[push.bufidx].outrefs[6].binding = 5;
  outbuf[push.bufidx].outrefs[6].idx = push.idx5;

  // terminator
  outbuf[push.bufidx].outrefs[7].binding = 100;
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(push_constant) uniform PushData
{
  uint bufidx;
} push;

struct tex_ref
{
  uint binding;
  uint idx;
};

layout(binding = 0, std430) buffer inbuftype {
  tex_ref inrefs[];
} inbuf[];

layout(binding = 1) uniform sampler2D tex1[)EOSHADER" STRINGIZE(DESC_ARRAY1_SIZE) R"EOSHADER(];
layout(binding = 2) uniform sampler2D tex2[];
layout(binding = 4) uniform sampler2D tex3[)EOSHADER" STRINGIZE(DESC_ARRAY3_SIZE) R"EOSHADER(];

layout(binding = 3, std430) buffer aliasbuf1type {
  vec4 Color;
  vec4 ignored;
  vec4 also_ignored;
} aliasbuf1[];

layout(binding = 3, std430) buffer aliasbuf2type {
  vec4 ignored;
  vec4 also_ignored;
  vec4 Color;
} aliasbuf2[];

void add_color(sampler2D tex)
{
  Color *= (vec4(0.25f) + texture(tex, vertIn.uv.xy));
}

void add_indirect_color2(sampler2D texs[)EOSHADER" STRINGIZE(DESC_ARRAY1_SIZE) R"EOSHADER(], uint idx)
{
  add_color(texs[idx]);
}

void add_indirect_color(int dummy,
                        sampler2D texs[)EOSHADER" STRINGIZE(DESC_ARRAY1_SIZE) R"EOSHADER(], tex_ref t)
{
  // second array-param function call
  add_indirect_color2(texs, t.idx);
}

void dispatch_indirect_color(int dummy1,
                             sampler2D texA[)EOSHADER" STRINGIZE(DESC_ARRAY1_SIZE) R"EOSHADER(],
                             sampler2D texB[)EOSHADER" STRINGIZE(DESC_ARRAY1_SIZE) R"EOSHADER(],
                             float dummy2, tex_ref t)
{
  if(t.binding == 0)
  {
    add_indirect_color(5, texA, t);
  }
  else
  {
    tex_ref t2 = t;
    t2.idx += 10;
    add_indirect_color(10, texB, t2);
  }
}

void add_parameterless()
{
  // use array directly without it being a function parameter
  Color += 0.1f * texture(tex1[)EOSHADER" STRINGIZE(INDEX3) R"EOSHADER(], vertIn.uv.xy);
}

layout(constant_id = 2) const int spec_canary = 0;

void main()
{
  if(spec_canary != 1338) { Color = vec4(1.0, 0.0, 0.0, 1.0); return; }

  if(vertIn.uv.y < 0.2f)
  {
    // nonuniform dynamic index
    Color = texture(tex1[nonuniformEXT(int(vertIn.col.w+0.5f))], vertIn.uv.xy);

    add_parameterless();
  }
  else
  {
    Color = vec4(vertIn.col.xyz, 1.0f);

    for(int i=0; i < 100; i++)
    {
      tex_ref t = inbuf[push.bufidx].inrefs[i];

      if(t.binding == 100)
        break;

      // function call with array parameters
      if(t.binding < 2)
        dispatch_indirect_color(0, tex1, tex1, 5.0f, t);
      else if(t.binding == 2)
        add_color(tex2[t.idx]);
      else if(t.binding == 3)
        add_color(tex3[t.idx]);
      else if(t.binding == 4)
        Color *= aliasbuf1[t.idx].Color;
      else if(t.binding == 5)
        Color *= aliasbuf2[t.idx].Color;
    }
  }
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    // dependencies of VK_EXT_descriptor_indexing
    devExts.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);

    features.fragmentStoresAndAtomics = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);

    // lazy - we could reduce this limit to a couple by not using combined image samplers
    if(props.limits.maxDescriptorSetSamplers < DESC_ARRAY1_SIZE + DESC_ARRAY2_SIZE)
      Avail = "maxDescriptorSetSamplers " + std::to_string(props.limits.maxDescriptorSetSamplers) +
              " is insufficient";
    else if(props.limits.maxDescriptorSetSampledImages < DESC_ARRAY1_SIZE + DESC_ARRAY2_SIZE)
      Avail = "maxDescriptorSetSampledImages " +
              std::to_string(props.limits.maxDescriptorSetSampledImages) + " is insufficient";

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexing = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    getPhysFeatures2(&descIndexing);

    if(!descIndexing.descriptorBindingPartiallyBound)
      Avail = "Descriptor indexing feature 'descriptorBindingPartiallyBound' not available";
    else if(!descIndexing.runtimeDescriptorArray)
      Avail = "Descriptor indexing feature 'runtimeDescriptorArray' not available";
    else if(!descIndexing.shaderSampledImageArrayNonUniformIndexing)
      Avail =
          "Descriptor indexing feature 'shaderSampledImageArrayNonUniformIndexing' not available";
    else if(!descIndexing.descriptorBindingVariableDescriptorCount)
      Avail =
          "Descriptor indexing feature 'descriptorBindingVariableDescriptorCount' not available";

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingEnable = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    descIndexingEnable.descriptorBindingPartiallyBound = VK_TRUE;
    descIndexingEnable.runtimeDescriptorArray = VK_TRUE;
    descIndexingEnable.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descIndexingEnable.descriptorBindingVariableDescriptorCount = VK_TRUE;

    devInfoNext = &descIndexingEnable;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    };

    VkDescriptorBindingFlagsEXT bindFlags[5] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
        0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT,
    };

    descFlags.bindingCount = ARRAY_COUNT(bindFlags);
    descFlags.pBindingFlags = bindFlags;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo(
            {
                {
                    0,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    DESC_ARRAY1_SIZE,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    1,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    DESC_ARRAY1_SIZE,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    2,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    DESC_ARRAY2_SIZE,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    3,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    DESC_ARRAY1_SIZE,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    4,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    UINT32_MAX,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
            })
            .next(&descFlags));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {
            setlayout,
        },
        {
            vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(Vec4i) * 2),
        }));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipelineShaderStageCreateInfo compshad =
        CompileShaderModule(comp, ShaderLang::glsl, ShaderStage::comp, "main");

    VkSpecializationMapEntry specmap[2] = {
        {1, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        {2, 1 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    uint32_t specvals[2] = {1337, 1338};

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = ARRAY_COUNT(specmap);
    spec.pMapEntries = specmap;
    spec.dataSize = sizeof(specvals);
    spec.pData = specvals;

    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;
    compshad.pSpecializationInfo = &spec;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    VkPipeline comppipe = createComputePipeline(vkh::ComputePipelineCreateInfo(layout, compshad));

    float left = float(NONUNIFORMIDX - 1.0f);
    float middle = float(NONUNIFORMIDX);
    float right = float(NONUNIFORMIDX + 1.0f);

    DefaultA2V tri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, left), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, middle), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, right), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(tri);

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(img.image, "Colour Tex");

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    float pixels[4 * 4 * 4];
    for(int i = 0; i < 4 * 4 * 4; i++)
      pixels[i] = RANDF(0.2f, 1.0f);

    AllocatedBuffer uploadBuf(
        this, vkh::BufferCreateInfo(sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    uploadBuf.upload(pixels);

    // create an image with black contents for all the indices we aren't using

    AllocatedImage badimg(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(badimg.image, "Black Tex");

    VkImageView badimgview = createImageView(vkh::ImageViewCreateInfo(
        badimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, badimg.image),
          });

      VkBufferImageCopy copy = {};
      copy.imageExtent = {4, 4, 1};
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.layerCount = 1;

      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             1, &copy);

      VkClearColorValue red = {{1.0f, 0.0f, 0.0f, 1.0f}};
      VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      vkCmdClearColorImage(cmd, badimg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &red, 1, &range);

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, img.image),
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, badimg.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

    VkDescriptorSet descset[5] = {};
    VkDescriptorPool descpool = VK_NULL_HANDLE;

    {
      CHECK_VKR(vkCreateDescriptorPool(
          device,
          vkh::DescriptorPoolCreateInfo(
              8,
              {
                  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESC_ARRAY2_SIZE * 10},
                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DESC_ARRAY1_SIZE * 20},
              }),
          NULL, &descpool));

      const static uint32_t numDescriptorSets = ARRAY_COUNT(descset);
      std::vector<VkDescriptorSetLayout> setLayouts(numDescriptorSets, setlayout);
      std::vector<uint32_t> counts(numDescriptorSets, DESC_ARRAY3_SIZE);

      VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
          NULL,
          numDescriptorSets,
          counts.data(),
      };

      VkDescriptorSetAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          &countInfo,
          descpool,
          numDescriptorSets,
          setLayouts.data(),
      };

      CHECK_VKR(vkAllocateDescriptorSets(device, &allocInfo, descset));
    }

    VkSampler sampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));

    // this corresponds to aliasbuf1type / aliasbuf2type
    Vec4f aliasbuf_data[3] = {};

    AllocatedBuffer alias_empty(this,
                                vkh::BufferCreateInfo(192, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    alias_empty.upload(aliasbuf_data);

    AllocatedBuffer alias1(this,
                           vkh::BufferCreateInfo(192, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    // first alias stores color first
    aliasbuf_data[0] = Vec4f(1.1f, 0.9f, 1.2f, 1.0f);

    alias1.upload(aliasbuf_data);

    AllocatedBuffer alias2(this,
                           vkh::BufferCreateInfo(192, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    // second alias stores color last
    aliasbuf_data[0] = Vec4f();
    aliasbuf_data[2] = Vec4f(1.1f, 0.9f, 1.2f, 1.0f);

    alias2.upload(aliasbuf_data);

    vkh::DescriptorBufferInfo bufinfo(alias_empty.buffer);
    vkh::DescriptorImageInfo iminfo(badimgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler);
    vkh::WriteDescriptorSet up(VK_NULL_HANDLE, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               {iminfo});

    std::vector<vkh::DescriptorImageInfo> ims;
    ims.resize(DESC_ARRAY2_SIZE, iminfo);

    up.pImageInfo = ims.data();

    std::vector<VkWriteDescriptorSet> ups;

    // fill the descriptor sets with values so they aren't all empty
    for(int i = 0; i < 5; i++)
    {
      up.dstSet = descset[i];

      up.dstBinding = 1;
      up.dstArrayElement = 0;
      up.descriptorCount = DESC_ARRAY1_SIZE;

      ups.push_back(up);

      // leave the first 20 elements empty
      up.dstBinding = 2;
      up.dstArrayElement = 20;
      up.descriptorCount = DESC_ARRAY2_SIZE - 20;

      ups.push_back(up);

      up.dstBinding = 4;
      up.dstArrayElement = 0;
      up.descriptorCount = DESC_ARRAY3_SIZE;

      ups.push_back(up);
    }

    vkh::updateDescriptorSets(device, ups);

    std::vector<vkh::DescriptorBufferInfo> bufs;
    bufs.resize(DESC_ARRAY1_SIZE, bufinfo);
    up.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    up.pBufferInfo = bufs.data();

    ups.clear();
    for(int i = 0; i < 5; i++)
    {
      up.dstSet = descset[i];

      up.dstBinding = 3;
      up.dstArrayElement = 0;
      up.descriptorCount = DESC_ARRAY1_SIZE;

      ups.push_back(up);
    }

    vkh::updateDescriptorSets(device, ups);

    AllocatedBuffer ssbo(this,
                         vkh::BufferCreateInfo(1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                         VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    // update the buffer only
    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset[0], 0, BUFIDX, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(ssbo.buffer)}),
                });

    // overwrite the indices we want with the right image
    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(
                descset[0], 1, INDEX3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
            vkh::WriteDescriptorSet(
                descset[0], 1, INDEX1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
            vkh::WriteDescriptorSet(
                descset[0], 1, INDEX1 + 10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
            vkh::WriteDescriptorSet(
                descset[0], 1, NONUNIFORMIDX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
            vkh::WriteDescriptorSet(
                descset[0], 2, INDEX2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
            vkh::WriteDescriptorSet(
                descset[0], 2, INDEX2 + 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
            vkh::WriteDescriptorSet(descset[0], 3, ALIAS1_INDEX, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(alias1.buffer)}),
            vkh::WriteDescriptorSet(descset[0], 3, ALIAS2_INDEX, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(alias2.buffer)}),
            vkh::WriteDescriptorSet(
                descset[0], 4, TEX3_INDEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                }),
        });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, comppipe);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descset[0], 0,
                              NULL);

      Vec4i idx[2] = {
          Vec4i(BUFIDX, INDEX1, INDEX2, TEX3_INDEX),
          Vec4i(ALIAS1_INDEX, ALIAS2_INDEX, 0, 0),
      };
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(idx), idx);

      static_assert(BUFIDX < DESC_ARRAY1_SIZE, "Buffer index is out of bounds");
      static_assert(INDEX1 < DESC_ARRAY1_SIZE, "Index 1 is out of bounds");
      static_assert(INDEX2 < DESC_ARRAY2_SIZE, "Index 2 is out of bounds");
      static_assert(TEX3_INDEX < DESC_ARRAY3_SIZE, "Index 3 is out of bounds");
      static_assert(ALIAS1_INDEX < DESC_ARRAY1_SIZE, "Alias index 1 is out of bounds");
      static_assert(ALIAS2_INDEX < DESC_ARRAY1_SIZE, "Alias index 2 is out of bounds");

      vkCmdFillBuffer(cmd, ssbo.buffer, 0, 1024 * 1024, 0);

      vkh::cmdPipelineBarrier(
          cmd, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                    ssbo.buffer)});

      // read the push constants, transform, pass them through the specified buffer to draw below
      vkCmdDispatch(cmd, 1, 1, 1);

      vkh::cmdPipelineBarrier(cmd, {},
                              {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                                        VK_ACCESS_SHADER_READ_BIT, ssbo.buffer)});

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

      // force all descriptor sets to be referenced
      for(int i = 0; i < ARRAY_COUNT(descset); i++)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset[i], 0,
                                NULL);

      // bind the actual one
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset[0], 0,
                              NULL);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      idx[0] = {BUFIDX, 0, 0, 0};
      idx[1] = {0, 0, 0, 0};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(idx), idx);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroyDescriptorPool(device, descpool, NULL);

    return 0;
  }
};

REGISTER_TEST();
