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

#define STRESS_TEST 0

#if STRESS_TEST

#define DESC_ARRAY1_SIZE 4096
#define DESC_ARRAY2_SIZE (1024 * 1024 - DESC_ARRAY1_SIZE)

#else

#define DESC_ARRAY1_SIZE 128
#define DESC_ARRAY2_SIZE (512)

#endif

#define BUFIDX 15
#define INDEX1 49
#define INDEX2 381
#define NONUNIFORMIDX 20

#define STRINGISE2(a) #a
#define STRINGISE(a) STRINGISE2(a)

TEST(VK_Descriptor_Indexing, VulkanGraphicsTest)
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

void main()
{
  outbuf[push.bufidx].outrefs[0].binding = 0;
  outbuf[push.bufidx].outrefs[0].idx = push.idx1;
  outbuf[push.bufidx].outrefs[1].binding = 2;
  outbuf[push.bufidx].outrefs[1].idx = push.idx2;

  outbuf[push.bufidx].outrefs[2].binding = 1;
  outbuf[push.bufidx].outrefs[2].idx = push.idx1;
  outbuf[push.bufidx].outrefs[3].binding = 2;
  outbuf[push.bufidx].outrefs[3].idx = push.idx2+5;

  // terminator
  outbuf[push.bufidx].outrefs[4].binding = 100;
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

layout(binding = 1) uniform sampler2D tex1[)EOSHADER" STRINGISE(DESC_ARRAY1_SIZE) R"EOSHADER(];
layout(binding = 2) uniform sampler2D tex2[];

void add_color(sampler2D tex)
{
  Color *= (vec4(0.25f) + texture(tex, vertIn.uv.xy));
}

void add_indirect_color2(sampler2D texs[)EOSHADER" STRINGISE(DESC_ARRAY1_SIZE) R"EOSHADER(], uint idx)
{
  add_color(texs[idx]);
}

void add_indirect_color(int dummy,
                        sampler2D texs[)EOSHADER" STRINGISE(DESC_ARRAY1_SIZE) R"EOSHADER(], tex_ref t)
{
  // second array-param function call
  add_indirect_color2(texs, t.idx);
}

void dispatch_indirect_color(int dummy1,
                             sampler2D texA[)EOSHADER" STRINGISE(DESC_ARRAY1_SIZE) R"EOSHADER(],
                             sampler2D texB[)EOSHADER" STRINGISE(DESC_ARRAY1_SIZE) R"EOSHADER(],
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

void main()
{
  if(vertIn.uv.y < 0.2f)
  {
    // nonuniform dynamic index
    Color = texture(tex1[nonuniformEXT(int(vertIn.col.w+0.5f))], vertIn.uv.xy);
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
      else
        add_color(tex2[t.idx]);
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

    devInfoNext = &descIndexing;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    };

    VkDescriptorBindingFlagsEXT bindFlags[3] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, 0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
    };

    descFlags.bindingCount = 3;
    descFlags.pBindingFlags = bindFlags;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo(
            {
                {
                    0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DESC_ARRAY1_SIZE,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESC_ARRAY1_SIZE,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                },
                {
                    2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESC_ARRAY2_SIZE,
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
                                   sizeof(Vec4i)),
        }));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    VkPipeline comppipe = createComputePipeline(vkh::ComputePipelineCreateInfo(
        layout, CompileShaderModule(comp, ShaderLang::glsl, ShaderStage::comp, "main")));

    float left = float(NONUNIFORMIDX - 1.0f);
    float middle = float(NONUNIFORMIDX);
    float right = float(NONUNIFORMIDX + 1.0f);

    DefaultA2V tri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, left), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, middle), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, right), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(allocator,
                       vkh::BufferCreateInfo(sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(tri);

    AllocatedImage img(allocator, vkh::ImageCreateInfo(
                                      4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(img.image, "Colour Tex");

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    float pixels[4 * 4 * 4];
    for(int i = 0; i < 4 * 4 * 4; i++)
      pixels[i] = RANDF(0.2f, 1.0f);

    AllocatedBuffer uploadBuf(
        allocator, vkh::BufferCreateInfo(sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    uploadBuf.upload(pixels);

    // create an image with black contents for all the indices we aren't using

    AllocatedImage badimg(allocator, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                              VK_IMAGE_USAGE_SAMPLED_BIT),
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
          device, vkh::DescriptorPoolCreateInfo(
                      8,
                      {
                          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESC_ARRAY2_SIZE * 10},
                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DESC_ARRAY1_SIZE * 10},
                      }),
          NULL, &descpool));

      CHECK_VKR(vkAllocateDescriptorSets(
          device, vkh::DescriptorSetAllocateInfo(
                      descpool, {setlayout, setlayout, setlayout, setlayout, setlayout}),
          descset));
    }

    VkSampler sampler = VK_NULL_HANDLE;

    VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(device, &sampInfo, NULL, &sampler);

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
    }

    vkh::updateDescriptorSets(device, ups);

    AllocatedBuffer ssbo(allocator,
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
        });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, comppipe);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descset[0], 0,
                              NULL);

      Vec4i idx = {BUFIDX, INDEX1, INDEX2, 0};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(Vec4i), &idx);

      static_assert(BUFIDX < DESC_ARRAY1_SIZE, "Buffer index is out of bounds");
      static_assert(INDEX1 < DESC_ARRAY1_SIZE, "Index 1 is out of bounds");
      static_assert(INDEX2 < DESC_ARRAY2_SIZE, "Index 2 is out of bounds");

      vkCmdFillBuffer(cmd, ssbo.buffer, 0, 1024 * 1024, 0);

      vkh::cmdPipelineBarrier(
          cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                             ssbo.buffer)});

      // read the push constants, transform, pass them through the specified buffer to draw below
      vkCmdDispatch(cmd, 1, 1, 1);

      vkh::cmdPipelineBarrier(
          cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                             ssbo.buffer)});

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
      idx = {BUFIDX, 0, 0, 0};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(Vec4i), &idx);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroyDescriptorPool(device, descpool, NULL);
    vkDestroySampler(device, sampler, NULL);

    return 0;
  }
};

REGISTER_TEST();