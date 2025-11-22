/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2025 Baldur Karlsson
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

RD_TEST(VK_Descriptor_Buffer, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test of EXT_descriptor_buffer based bindings and different edge cases.";

  VkPhysicalDeviceDescriptorBufferFeaturesEXT descBufFeatures = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
  };
  VkPhysicalDeviceDescriptorBufferPropertiesEXT descBufProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
  };

  std::string header = R"EOSHADER(

#version 460 core

#extension GL_EXT_samplerless_texture_functions : require

)EOSHADER";

  std::string pixel = R"EOSHADER(

layout(push_constant) uniform PushData {
  vec4 data;
} push;

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 1, std140) uniform aa
{
  vec4 data[90];
} a;

layout(set = 0, binding = 2, std140) buffer bb
{
  vec4 data[90];
} b;

layout(set = 0, binding = 11) uniform samplerBuffer c;
layout(set = 0, binding = 12, rgba32f) uniform imageBuffer d;

layout(set = 0, binding = 21) uniform texture2D e;
layout(set = 0, binding = 22, rgba8) uniform image2D f;
layout(set = 0, binding = 23, input_attachment_index = 0) uniform subpassInput g;

layout(set = 0, binding = 31) uniform sampler h;

layout(set = 0, binding = 41) uniform sampler2D i;

#ifdef RAYS
layout(set = 0, binding = 51) uniform accelerationStructureEXT j;
#endif

layout(set = 0, binding = 61, std140) uniform descbuff
{
  vec4 data[3];
} descbuf;

layout(set = 1, binding = 0) uniform sampler l;

layout(set = 3, binding = 1, std140) uniform mm
{
  vec4 data[90];
} m[100];

layout(set = 3, binding = 2) uniform sampler2D n[100];

#ifdef RAYS
layout(set = 3, binding = 3) uniform accelerationStructureEXT u[100];
#endif

layout(set = 3, binding = 4, std140) uniform oo
{
  vec4 data[90];
} o[];

layout(set = 4, binding = 1, std140) uniform pp
{
  vec4 data[90];
} p;

layout(set = 4, binding = 2, std140) buffer qq
{
  vec4 data[90];
} q;

layout(set = 5, binding = 0) uniform sampler r;

layout(set = 2, binding = 0) uniform sampler t_samp[100];
layout(set = 2, binding = 0) uniform texture2D t_tex[100];
layout(set = 2, binding = 0) uniform sampler2D t_comb[100];
layout(set = 2, binding = 0, std140) uniform tt_ubo
{
  vec4 data[90];
} t_ubo[];
layout(set = 2, binding = 0, std140) buffer tt_ssbo
{
  vec4 data[90];
} t_ssbo[];
#ifdef RAYS
layout(set = 2, binding = 0) uniform accelerationStructureEXT t_as[100];
#endif

void main()
{
  vec2 uv = vec2(gl_FragCoord.xy - ivec2(push.data.zw))/push.data.xx;
  ivec2 uvi = ivec2(uv*push.data.yy);

  Color = vec4(uv.xy, 0.0f, 1.0f);

#if defined(RAYS)
  const vec3 light_origin = vec3(0,0,0);

  const vec3  pos = vec3((uv.xy - 0.5f)*10*vec2(1,-1), 5.0f);

  const float tmin = 0.01, tmax = 1000;
  const vec3  direction = light_origin - pos;

  rayQueryEXT query;
  float blue = 0.0f;
#if RAYS == 1
  rayQueryInitializeEXT(query, j, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, pos, tmin, direction.xyz, 1.0);
#elif RAYS == 2
  blue = 1.0f;
  rayQueryInitializeEXT(query, t_as[60], gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, pos, tmin, direction.xyz, 1.0);
#elif RAYS == 3
  blue = 0.2f;
  rayQueryInitializeEXT(query, u[20], gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, pos, tmin, direction.xyz, 1.0);
#elif RAYS == 4
  blue = 0.0f;
  rayQueryInitializeEXT(query, u[31], gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, pos, tmin, direction.xyz, 1.0);
#endif
  rayQueryProceedEXT(query);

  if(rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
    Color = vec4(0, 1, blue, 1);
  else
    Color = vec4(1, 0, blue, 1);
#elif TEST == 0
  Color = a.data[1] + a.data[79];
#elif TEST == 1
  Color = b.data[1] + b.data[79];
#elif TEST == 2
  Color = texelFetch(c, 1);
#elif TEST == 3
  Color = imageLoad(d, 1);
#elif TEST == 4
  Color = texelFetch(e, uvi, 0);
#elif TEST == 5
  Color = imageLoad(f, uvi);
#elif TEST == 6
  Color = subpassLoad(g);
#elif TEST == 7
  Color = textureLod(sampler2D(e, h), uv, 0.0);
#elif TEST == 8
  Color = texture(i, uv);
#elif TEST == 9
  // j - rays
#elif TEST == 10
  // inline UBO, named 'descbuf' instead of k to match resource name
  // we don't do a robustness check because inline UBOs don't provide bounds checking
  Color = descbuf.data[1];
#elif TEST == 11
  Color = textureLod(sampler2D(e, l), uv, 0.0);
#elif TEST == 12
  Color = m[20].data[1] + m[20].data[79];
#elif TEST == 13
  Color = m[31].data[1] + m[31].data[79];
#elif TEST == 14
  Color = textureLod(n[20], uv, 0.0);
#elif TEST == 15
  Color = textureLod(n[31], uv, 0.0);
#elif TEST == 16
  Color = textureLod(n[41], uv, 0.0);
#elif TEST == 17
  Color = o[40].data[1] + o[40].data[79];
#elif TEST == 18
  Color = o[51].data[1] + o[51].data[79];
#elif TEST == 19
  Color = p.data[1] + p.data[79];
#elif TEST == 20
  Color = q.data[1] + q.data[79];
#elif TEST == 21
  Color = textureLod(sampler2D(e, r), uv, 0.0);
#elif TEST == 22

#if defined(MUTABLE_SAMP)
  Color = textureLod(sampler2D(t_tex[20], t_samp[10]), uv, 0.0);
#else
  Color = textureLod(sampler2D(t_tex[20], r), uv, 0.0);
#endif

#elif TEST == 23 && defined(MUTABLE_COMB)
  Color = texture(t_comb[30], uv);
#elif TEST == 24
  Color = t_ubo[40].data[1] + t_ubo[40].data[79];
#elif TEST == 25
  Color = t_ssbo[50].data[1] + t_ssbo[50].data[79];
#endif
}

)EOSHADER";

  static const uint32_t NUM_TESTS = 26;

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
    devExts.push_back(VK_KHR_MAINTENANCE_1_EXTENSION_NAME);
    devExts.push_back(VK_KHR_MAINTENANCE_6_EXTENSION_NAME);
    devExts.push_back(VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME);
    devExts.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
    devExts.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    devExts.push_back(VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME);
    devExts.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
    devExts.push_back(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);

    // Required for ray queries
    optDevExts.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

    // Required by VK_KHR_spirv_1_4
    optDevExts.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

    optFeatures.sparseBinding = VK_TRUE;
    optFeatures.sparseResidencyBuffer = VK_TRUE;
    optFeatures.sparseResidencyImage2D = VK_TRUE;

    features.fragmentStoresAndAtomics = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    getPhysFeatures2(&descBufFeatures);
    getPhysProperties2(&descBufProps);

    if(!descBufFeatures.descriptorBuffer)
      Avail = "Feature 'descriptorBuffer' not available";

    descBufFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &descBufFeatures;

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufaddrFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };

    getPhysFeatures2(&bufaddrFeatures);

    if(!bufaddrFeatures.bufferDeviceAddress)
      Avail = "feature 'bufferDeviceAddress' not available";

    bufaddrFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &bufaddrFeatures;

    static VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
    };

    getPhysFeatures2(&ycbcrFeats);

    if(!ycbcrFeats.samplerYcbcrConversion)
      Avail = "feature 'samplerYcbcrConversion' not available";

    ycbcrFeats.pNext = (void *)devInfoNext;
    devInfoNext = &ycbcrFeats;

    static VkPhysicalDeviceMaintenance6Features maint6Feats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES,
    };

    getPhysFeatures2(&maint6Feats);

    if(!maint6Feats.maintenance6)
      Avail = "feature 'maintenance6' not available";

    maint6Feats.pNext = (void *)devInfoNext;
    devInfoNext = &maint6Feats;

    static VkPhysicalDeviceRobustness2FeaturesEXT robustFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
    };

    getPhysFeatures2(&robustFeats);

    if(!robustFeats.nullDescriptor)
      Avail = "feature 'nullDescriptor' not available";

    robustFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
    };
    robustFeats.nullDescriptor = VK_TRUE;

    robustFeats.pNext = (void *)devInfoNext;
    devInfoNext = &robustFeats;

    static VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT,
    };

    getPhysFeatures2(&inlineFeatures);

    if(!inlineFeatures.inlineUniformBlock)
      Avail = "feature 'inlineUniformBlock' not available";

    inlineFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &inlineFeatures;

    static VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT,
    };

    getPhysFeatures2(&mutableFeatures);

    if(!mutableFeatures.mutableDescriptorType)
      Avail = "feature 'mutableDescriptorType' not available";

    mutableFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &mutableFeatures;

    static VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    };

    static VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    };

    if(hasExt(VK_KHR_RAY_QUERY_EXTENSION_NAME))
    {
      getPhysFeatures2(&accelFeats);

      if(!accelFeats.accelerationStructure)
        Avail = "feature 'accelerationStructure' not available";

      accelFeats.pNext = (void *)devInfoNext;
      devInfoNext = &accelFeats;

      getPhysFeatures2(&rqFeatures);

      if(!rqFeatures.rayQuery)
        Avail = "Ray query feature 'rayQuery' not available";

      rqFeatures.pNext = (void *)devInfoNext;
      devInfoNext = &rqFeatures;
    }

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingEnable = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    descIndexingEnable.runtimeDescriptorArray = VK_TRUE;
    descIndexingEnable.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
    descIndexingEnable.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descIndexingEnable.descriptorBindingVariableDescriptorCount = VK_TRUE;

    descIndexingEnable.pNext = (void *)devInfoNext;
    devInfoNext = &descIndexingEnable;
  }

  byte *descWrite;

  VkDeviceAddress dataAddress;
  VkDeviceSize setOffset;

  bool mutableSet = false;

  std::vector<VkDescriptorType> mutableTypes = {
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
  };

  size_t DescSize(VkDescriptorType type)
  {
    if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      return descBufProps.uniformBufferDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      return descBufProps.storageBufferDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
      return descBufProps.uniformTexelBufferDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      return descBufProps.storageTexelBufferDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
      return descBufProps.sampledImageDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
      return descBufProps.storageImageDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      return descBufProps.inputAttachmentDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_SAMPLER)
      return descBufProps.samplerDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      return descBufProps.combinedImageSamplerDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      return descBufProps.accelerationStructureDescriptorSize;

    return 0;
  }

  size_t DescStride(VkDescriptorType type)
  {
    if(mutableSet)
    {
      size_t ret = 0;
      for(VkDescriptorType t : mutableTypes)
        ret = std::max(ret, DescSize(t));
      return ret;
    }

    return DescSize(type);
  }

  struct BindRef
  {
    uint32_t bind;
    uint32_t idx;

    BindRef(uint32_t b) : bind(b), idx(0) {}
    BindRef(std::initializer_list<uint32_t> bind_idx)
        : bind(bind_idx.begin()[0]), idx(bind_idx.begin()[1])
    {
    }
  };

  void FillDescriptor(VkDescriptorSetLayout layout, BindRef bind, VkDescriptorType type,
                      VkDeviceSize offset, VkDeviceSize range, VkFormat format = VK_FORMAT_UNDEFINED)
  {
    VkDeviceSize bindOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, bind.bind, &bindOffset);
    void *dst = descWrite + setOffset + bindOffset + DescStride(type) * bind.idx;

    VkDescriptorGetInfoEXT get = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    get.type = type;

    VkDescriptorAddressInfoEXT buf = {VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};
    buf.address = dataAddress + offset;
    buf.range = range;
    buf.format = format;
    get.data.pStorageBuffer = &buf;

    vkGetDescriptorEXT(device, &get, DescSize(type), dst);
  }

  void FillDescriptor(VkDescriptorSetLayout layout, BindRef bind, VkAccelerationStructureKHR as)
  {
    VkDeviceSize bindOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, bind.bind, &bindOffset);
    void *dst = descWrite + setOffset + bindOffset +
                DescStride(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) * bind.idx;

    VkDescriptorGetInfoEXT get = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    get.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    VkAccelerationStructureDeviceAddressInfoKHR info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
    };
    info.accelerationStructure = as;
    get.data.accelerationStructure = vkGetAccelerationStructureDeviceAddressKHR(device, &info);

    vkGetDescriptorEXT(device, &get, DescSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR), dst);
  }

  void FillDescriptor(VkDescriptorSetLayout layout, BindRef bind, VkDescriptorType type,
                      VkSampler sampler, VkImageView view)
  {
    VkDeviceSize bindOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, bind.bind, &bindOffset);
    void *dst = descWrite + setOffset + bindOffset + DescStride(type) * bind.idx;

    VkDescriptorGetInfoEXT get = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    get.type = type;

    VkDescriptorImageInfo im;
    im.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    im.imageView = view;
    im.sampler = sampler;

    if(type == VK_DESCRIPTOR_TYPE_SAMPLER)
      get.data.pSampler = &sampler;
    else
      get.data.pCombinedImageSampler = &im;

    vkGetDescriptorEXT(device, &get, DescSize(type), dst);
  }

  void FillDescriptor(VkDescriptorSetLayout layout, BindRef bind, VkDescriptorType type)
  {
    VkDeviceSize bindOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, bind.bind, &bindOffset);
    void *dst = descWrite + setOffset + bindOffset + DescSize(type) * bind.idx;

    VkDescriptorGetInfoEXT get = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    get.type = type;

    vkGetDescriptorEXT(device, &get, DescSize(type), dst);
  }

  AllocatedBuffer MakeTestBuffer(const char *name, uint32_t offset, const Vec4f &data)
  {
    // use 256 aligned sizes for buffers so we can check this on all drivers, we don't care to test
    // aliasing caused by different sizes
    VkDeviceSize size = AlignUp(offset, 0x100U) + 0x2000;
    AllocatedBuffer ret(this,
                        vkh::BufferCreateInfo(size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    setName(ret.buffer, name);
    dataAddress = ret.address;
    byte *ptr = ret.map();
    // fill with garbage (that will be a relatively normal float value)
    memset(ptr, 0x3f, size);
    memcpy(ptr + offset, &data, sizeof(data));
    ret.unmap();

    return ret;
  }

  static const uint32_t texSize = 4;

  VkImageView MakeTestImage(const char *name, const Vec4f &col)
  {
    // make images half one colour half black, so we can test samplers that are linear vs point
    Vec4f pixels[texSize * texSize] = {};

    static AllocatedBuffer uploadBuf(
        this,
        vkh::BufferCreateInfo(texSize * texSize * sizeof(Vec4f), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    AllocatedImage tex(
        this,
        vkh::ImageCreateInfo(texSize, texSize, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo(
            {VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(tex.image, name);

    for(int i = 0; i < texSize * texSize / 2; i++)
      pixels[i] = col;
    uploadBuf.upload(pixels);
    uploadBufferToImage(tex.image, {texSize, texSize, 1}, uploadBuf.buffer, VK_IMAGE_LAYOUT_GENERAL);

    return createImageView(
        vkh::ImageViewCreateInfo(tex.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));
  }

  int main()
  {
    vmaBDA = true;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool rays = hasExt(VK_KHR_RAY_QUERY_EXTENSION_NAME);

    VkDescriptorType asDescType =
        rays ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    VkDescriptorSetLayout singlesetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
            {
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

                {11, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {12, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

                {21, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {22, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {23, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

                {31, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

                {41, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

                {51, asDescType, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

                {61, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 32, VK_SHADER_STAGE_FRAGMENT_BIT},
            },
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    VkDescriptorSetLayout samplayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
        {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        },
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    VkDescriptorBindingFlagsEXT bindFlags[] = {
        0,
        0,
        0,
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    };
    descFlags.bindingCount = ARRAY_COUNT(bindFlags);
    descFlags.pBindingFlags = bindFlags;

    VkDescriptorSetLayout arraysetlayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo(
            {
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100, VK_SHADER_STAGE_FRAGMENT_BIT},
                {3, asDescType, 100, VK_SHADER_STAGE_FRAGMENT_BIT},
                {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000000, VK_SHADER_STAGE_FRAGMENT_BIT},
            },
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
            .next(&descFlags));

    bool mutableComb = false, mutableAS = false, mutableSamp = false;

    {
      VkDescriptorType queryTypes[2] = {
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          VK_DESCRIPTOR_TYPE_SAMPLER,
      };
      VkMutableDescriptorTypeListEXT mutableList = {
          2,
          queryTypes,
      };
      VkMutableDescriptorTypeCreateInfoEXT mutableTypeInfo = {
          VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
          NULL,
          1,
          &mutableList,
      };

      std::vector<VkDescriptorSetLayoutBinding> bindings = {
          {0, VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      };
      VkDescriptorSetLayoutCreateInfo createInfo =
          vkh::DescriptorSetLayoutCreateInfo(
              bindings, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
              .next(&mutableTypeInfo);

      VkDescriptorSetLayoutSupport support = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT};

      queryTypes[1] = VK_DESCRIPTOR_TYPE_SAMPLER;
      vkGetDescriptorSetLayoutSupport(device, &createInfo, &support);
      mutableSamp = support.supported != VK_FALSE;
      if(mutableSamp)
      {
        mutableTypes.push_back(VK_DESCRIPTOR_TYPE_SAMPLER);
        header += "#define MUTABLE_SAMP 1\n";

        TEST_LOG("Mutable samplers are supported");
      }

      queryTypes[1] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      vkGetDescriptorSetLayoutSupport(device, &createInfo, &support);
      mutableComb = support.supported != VK_FALSE;
      if(mutableComb)
      {
        mutableTypes.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        header += "#define MUTABLE_COMB 1\n";

        TEST_LOG("Mutable combined image/samplers are supported");
      }

      if(rays)
      {
        queryTypes[1] = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        vkGetDescriptorSetLayoutSupport(device, &createInfo, &support);
        mutableAS = support.supported != VK_FALSE;
        if(mutableAS)
        {
          mutableTypes.push_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
          TEST_LOG("Mutable ASs are supported");
        }
      }
    }

    VkMutableDescriptorTypeListEXT mutableList = {
        uint32_t(mutableTypes.size()),
        mutableTypes.data(),
    };
    VkMutableDescriptorTypeCreateInfoEXT mutableTypeInfo = {
        VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
        NULL,
        1,
        &mutableList,
    };

    VkDescriptorSetLayout mutablelayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo(
            {
                {0, VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 100, VK_SHADER_STAGE_FRAGMENT_BIT},
            },
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
            .next(&mutableTypeInfo));

    // we need each sampler to be different in a way that can't be deduplicated and aliased
    vkh::SamplerCreateInfo sampInfo(VK_FILTER_LINEAR);

    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkSampler h = createSampler(sampInfo);
    setName(h, "h");

    sampInfo.magFilter = VK_FILTER_NEAREST;

    VkSampler i_samp = createSampler(sampInfo);
    setName(i_samp, "i_samp");

    sampInfo.minFilter = VK_FILTER_NEAREST;

    VkSampler l = createSampler(sampInfo);
    setName(l, "l");

    sampInfo.magFilter = VK_FILTER_LINEAR;

    VkSampler n_20_samp = createSampler(sampInfo);
    setName(n_20_samp, "n_20_samp");

    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkSampler n_31_samp = createSampler(sampInfo);
    setName(n_31_samp, "n_31_samp");

    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkSampler n_41_samp = createSampler(sampInfo);
    setName(n_41_samp, "n_41_samp");

    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkSampler r = createSampler(sampInfo);
    setName(r, "r");

    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VkSampler t_samp_10 = createSampler(sampInfo);
    setName(t_samp_10, "t_samp_10");

    sampInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSampler t_comb_30_samp = createSampler(sampInfo);
    setName(t_comb_30_samp, "t_comb_30_samp");

    VkDescriptorSetLayout immutsetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
            {
                {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &r},
            },
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
                VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT));

    VkDescriptorSetLayout pushlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
        {
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        },
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR |
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {singlesetlayout, samplayout, mutablelayout, arraysetlayout, pushlayout, immutsetlayout},
        {
            vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i)),
        }));

    // because some devices don't support more than one sampler heap, and we definitely want to test
    // combined image/samplers, we just test with one descriptor buffer by default and just add a
    // stupid sampler-only heap to test multiple heaps
    AllocatedBuffer descbuf(
        this,
        vkh::BufferCreateInfo(0x100000, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                                            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    setName(descbuf.buffer, "descbuf");

    AllocatedBuffer sampbuf;
    if(descBufProps.maxSamplerDescriptorBufferBindings > 1)
    {
      sampbuf = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(0x100000, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                              VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
      setName(sampbuf.buffer, "sampbuf");
    }

    AllocatedBuffer pushbuf;
    if(descBufProps.bufferlessPushDescriptors == VK_FALSE)
    {
      pushbuf = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(0x100000,
                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                    VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT |
                                    VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
      setName(pushbuf.buffer, "pushbuf");
    }

    byte *descs = descbuf.map();
    // ensure that we never read 0s except from a NULL descriptor
    memset(descs, 0xcc, 0x100000);

    AllocatedImage input(
        this,
        vkh::ImageCreateInfo(screenWidth, screenHeight, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT),
        VmaAllocationCreateInfo(
            {VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(input.image, "g");

    VkImageView g = createImageView(vkh::ImageViewCreateInfo(input.image, VK_IMAGE_VIEW_TYPE_2D,
                                                             VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedImage colatt(
        this,
        vkh::ImageCreateInfo(screenWidth, screenHeight, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        VmaAllocationCreateInfo(
            {VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(colatt.image, "colatt");

    VkImageView colview = createImageView(vkh::ImageViewCreateInfo(
        colatt.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));
    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_LOAD));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})},
                                    VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, {},
                                    {VkAttachmentReference({1, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {colview, g}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    pipeCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    std::vector<VkPipeline> tests;

    pipeCreateInfo.stages.resize(2);
    pipeCreateInfo.stages[0] =
        CompileShaderModule(VKFullscreenQuadVertex, ShaderLang::glsl, ShaderStage::vert, "main");

    for(uint32_t i = 0; i < NUM_TESTS; i++)
    {
      pipeCreateInfo.stages[1] =
          CompileShaderModule(header + "#define TEST " + std::to_string(i) + pixel,
                              ShaderLang::glsl, ShaderStage::frag, "main");

      tests.push_back(createGraphicsPipeline(pipeCreateInfo));
    }

    if(rays)
    {
      pipeCreateInfo.stages[1] = CompileShaderModule(header +
                                                         "\n"
                                                         "#extension GL_EXT_ray_query : enable\n"
                                                         " #define RAYS 1 \n" +
                                                         pixel,
                                                     ShaderLang::glsl, ShaderStage::frag, "main");

      tests.push_back(createGraphicsPipeline(pipeCreateInfo));

      if(mutableAS)
      {
        pipeCreateInfo.stages[1] = CompileShaderModule(header +
                                                           "\n"
                                                           "#extension GL_EXT_ray_query : enable\n"
                                                           " #define RAYS 2 \n" +
                                                           pixel,
                                                       ShaderLang::glsl, ShaderStage::frag, "main");

        tests.push_back(createGraphicsPipeline(pipeCreateInfo));
      }

      pipeCreateInfo.stages[1] = CompileShaderModule(header +
                                                         "\n"
                                                         "#extension GL_EXT_ray_query : enable\n"
                                                         " #define RAYS 3 \n" +
                                                         pixel,
                                                     ShaderLang::glsl, ShaderStage::frag, "main");

      tests.push_back(createGraphicsPipeline(pipeCreateInfo));

      pipeCreateInfo.stages[1] = CompileShaderModule(header +
                                                         "\n"
                                                         "#extension GL_EXT_ray_query : enable\n"
                                                         " #define RAYS 4 \n" +
                                                         pixel,
                                                     ShaderLang::glsl, ShaderStage::frag, "main");

      tests.push_back(createGraphicsPipeline(pipeCreateInfo));
    }

    VkImageView e = MakeTestImage("e", Vec4f(1.0f, 0.0f, 0.0f, 1.0f));
    VkImageView f = MakeTestImage("f", Vec4f(0.0f, 1.0f, 0.0f, 1.0f));
    VkImageView i_tex = MakeTestImage("i_tex", Vec4f(1.0f, 0.0f, 1.0f, 1.0f));
    VkImageView n_20_tex = MakeTestImage("n_20_tex", Vec4f(1.0f, 1.0f, 0.0f, 1.0f));
    VkImageView t_tex_20 = MakeTestImage("t_tex_20", Vec4f(0.0f, 1.0f, 1.0f, 1.0f));
    VkImageView t_comb_30_tex = MakeTestImage("t_comb_30_tex", Vec4f(0.5f, 0.0f, 0.5f, 1.0f));

    AllocatedBuffer blasBuffer;
    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    AllocatedBuffer tlasBuffer;
    VkAccelerationStructureKHR j = VK_NULL_HANDLE;
    VkAccelerationStructureKHR t_as_60 = VK_NULL_HANDLE;
    VkAccelerationStructureKHR u_20 = VK_NULL_HANDLE;
    if(rays)
    {
      Vec3f vertices[] = {
          // Triangle
          {0.0f, 0.3f, 0.5f},
          {-0.3f, -0.3f, 0.5f},
          {0.3f, -0.3f, 0.5f},
      };

      uint32_t indices[] = {0, 1, 2};

      uint32_t primitiveCount = (uint32_t)sizeof(indices) / (sizeof(indices[0]) * 3);
      uint32_t indexCount = (uint32_t)sizeof(indices) / sizeof(indices[0]);
      uint32_t vertexCount = (uint32_t)sizeof(vertices) / sizeof(vertices[0]);

      VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

      constexpr VkBufferUsageFlags blasInputBufferUsageFlags =
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;

      VkTransformMatrixKHR identityTransformMatrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                                      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

      VkTransformMatrixKHR blasTransformMatrix = identityTransformMatrix;

      const size_t vertexBufferSize = vertexCount * sizeof(vertices[0]);
      const size_t indexBufferSize = indexCount * sizeof(indices[0]);

      AllocatedBuffer blasVertexBuffer(
          this, vkh::BufferCreateInfo(vertexBufferSize, blasInputBufferUsageFlags),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}), 4);
      AllocatedBuffer blasIndexBuffer(
          this, vkh::BufferCreateInfo(indexBufferSize, blasInputBufferUsageFlags),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}), 4);

      blasVertexBuffer.upload(vertices, vertexBufferSize);
      blasIndexBuffer.upload(indices, indexBufferSize);

      /*
       * Create bottom level acceleration structure
       */
      VkAccelerationStructureGeometryKHR blasGeometry = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
      blasGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
      blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      blasGeometry.geometry.triangles.sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
      blasGeometry.geometry.triangles.vertexFormat = vertexFormat;
      blasGeometry.geometry.triangles.maxVertex = vertexCount - 1;
      blasGeometry.geometry.triangles.vertexStride = sizeof(Vec3f);
      blasGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
      blasGeometry.geometry.triangles.vertexData.deviceAddress = blasVertexBuffer.address;
      blasGeometry.geometry.triangles.indexData.deviceAddress = blasIndexBuffer.address;
      blasGeometry.geometry.triangles.transformData.deviceAddress = 0;
      std::vector<VkAccelerationStructureGeometryKHR> blasGeometries = {blasGeometry};

      VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {primitiveCount, 0, 0, 0};

      std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfosVector = {buildRangeInfo};
      VkAccelerationStructureBuildRangeInfoKHR *asBuildRangeInfos = asBuildRangeInfosVector.data();
      std::vector<uint32_t> primitiveCounts = {primitiveCount};

      VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
      blasBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      blasBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
      blasBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      blasBuildGeometryInfo.geometryCount = (uint32_t)blasGeometries.size();
      blasBuildGeometryInfo.pGeometries = blasGeometries.data();

      VkAccelerationStructureBuildSizesInfoKHR blasBuildSizesInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
      vkGetAccelerationStructureBuildSizesKHR(
          device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasBuildGeometryInfo,
          primitiveCounts.data(), &blasBuildSizesInfo);

      blasBuffer = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(blasBuildSizesInfo.accelerationStructureSize,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
      blasCreateInfo.buffer = blasBuffer.buffer;
      blasCreateInfo.size = blasBuildSizesInfo.accelerationStructureSize;
      blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

      CHECK_VKR(vkCreateAccelerationStructureKHR(device, &blasCreateInfo, VK_NULL_HANDLE, &blas))
      setName(blas, "blas");

      VkAccelerationStructureDeviceAddressInfoKHR blasDeviceAddressInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, NULL, blas};
      uint64_t blasDeviceAddress =
          vkGetAccelerationStructureDeviceAddressKHR(device, &blasDeviceAddressInfo);

      AllocatedBuffer blasScratchBuffer(
          this,
          vkh::BufferCreateInfo(
              blasBuildSizesInfo.buildScratchSize,
              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}), 256);

      blasBuildGeometryInfo.scratchData.deviceAddress = blasScratchBuffer.address;
      blasBuildGeometryInfo.dstAccelerationStructure = blas;

      {
        VkCommandBuffer cmd = GetCommandBuffer();
        CHECK_VKR(vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo()));
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuildGeometryInfo, &asBuildRangeInfos);
        CHECK_VKR(vkEndCommandBuffer(cmd));
        Submit(99, 99, {cmd});
      }

      /*
       * Create top level acceleration structure
       */
      VkTransformMatrixKHR tlasTransformMatrix = identityTransformMatrix;

      VkAccelerationStructureInstanceKHR asInstance = {
          tlasTransformMatrix,
          0,
          0xFF,
          0,
          VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
          blasDeviceAddress,
      };

      const size_t asInstanceSize = sizeof(asInstance);

      AllocatedBuffer instancesBuffer(
          this,
          vkh::BufferCreateInfo(
              asInstanceSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}), 16);
      instancesBuffer.upload(&asInstance, asInstanceSize);

      VkAccelerationStructureGeometryKHR tlasGeometry = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
      tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
      tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      tlasGeometry.geometry.instances.sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
      tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
      tlasGeometry.geometry.instances.data.deviceAddress = instancesBuffer.address;

      std::vector<VkAccelerationStructureGeometryKHR> tlasGeometries = {tlasGeometry};

      VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
      tlasBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      tlasBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
      tlasBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      tlasBuildGeometryInfo.geometryCount = (uint32_t)tlasGeometries.size();
      tlasBuildGeometryInfo.pGeometries = tlasGeometries.data();

      VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
      vkGetAccelerationStructureBuildSizesKHR(
          device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo,
          primitiveCounts.data(), &tlasBuildSizesInfo);

      tlasBuffer = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(tlasBuildSizesInfo.accelerationStructureSize + 0x2000,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
      tlasCreateInfo.buffer = tlasBuffer.buffer;
      tlasCreateInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
      tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

      CHECK_VKR(vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, VK_NULL_HANDLE, &j));
      setName(j, "j");

      tlasCreateInfo.offset = 0x1000;

      CHECK_VKR(vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, VK_NULL_HANDLE, &t_as_60));
      setName(t_as_60, "t_as_60");

      tlasCreateInfo.offset = 0x2000;

      CHECK_VKR(vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, VK_NULL_HANDLE, &u_20));
      setName(u_20, "u_20");

      AllocatedBuffer tlasScratchBuffer(
          this,
          vkh::BufferCreateInfo(
              tlasBuildSizesInfo.buildScratchSize + 0x2000,
              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}), 256);

      tlasBuildGeometryInfo.scratchData.deviceAddress = tlasScratchBuffer.address;
      tlasBuildGeometryInfo.dstAccelerationStructure = j;

      asBuildRangeInfosVector[0].primitiveCount = 1;

      {
        VkCommandBuffer cmd = GetCommandBuffer();
        CHECK_VKR(vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo()));
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildGeometryInfo, &asBuildRangeInfos);
        tlasBuildGeometryInfo.dstAccelerationStructure = t_as_60;
        tlasBuildGeometryInfo.scratchData.deviceAddress = tlasScratchBuffer.address + 0x1000;
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildGeometryInfo, &asBuildRangeInfos);
        tlasBuildGeometryInfo.dstAccelerationStructure = u_20;
        tlasBuildGeometryInfo.scratchData.deviceAddress = tlasScratchBuffer.address + 0x2000;
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildGeometryInfo, &asBuildRangeInfos);
        CHECK_VKR(vkEndCommandBuffer(cmd));
        Submit(99, 99, {cmd});
      }
    }

    uint32_t bufIdxs[6] = {};
    VkDeviceSize setOffsets[6];

    {
      // single set
      setOffsets[0] = 0;

      VkDeviceSize sz = 0;
      vkGetDescriptorSetLayoutSizeEXT(device, singlesetlayout, &sz);

      // sampler set
      setOffsets[1] = setOffsets[0] + std::max(sz, (VkDeviceSize)0x4000ULL);

      vkGetDescriptorSetLayoutSizeEXT(device, samplayout, &sz);

      // mutable set
      setOffsets[2] = setOffsets[1] + std::max(sz, (VkDeviceSize)0x400ULL);

      vkGetDescriptorSetLayoutSizeEXT(device, mutablelayout, &sz);

      // array set
      setOffsets[3] = setOffsets[2] + std::max(sz, (VkDeviceSize)0x4000ULL);

      vkGetDescriptorSetLayoutSizeEXT(device, arraysetlayout, &sz);
    }

    VkDescriptorBufferBindingInfoEXT descBind[3] = {};
    VkDescriptorBufferBindingPushDescriptorBufferHandleEXT pushbufHandle = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT,
    };

    uint32_t numBufs = 1;
    descBind[0] = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
        NULL,
        descbuf.address,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
    };
    if(sampbuf.address)
    {
      bufIdxs[1] = numBufs;
      setOffsets[1] = 0;

      descBind[numBufs] = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
          NULL,
          sampbuf.address,
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
              VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
      };
      numBufs++;
    }
    if(pushbuf.address)
    {
      bufIdxs[4] = numBufs;

      descBind[numBufs] = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
          NULL,
          sampbuf.address,
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
              VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT |
              VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
      };
      pushbufHandle.buffer = pushbuf.buffer;
      descBind[numBufs].pNext = &pushbufHandle;
      numBufs++;
    }

    ////////////// set 0  ////////////////
    ////////////// single ////////////////
    descWrite = descs;
    setOffset = setOffsets[0];

    MakeTestBuffer("a", 0x310, Vec4f(1.0f, 2.0f, 3.0f, 4.0f));
    FillDescriptor(singlesetlayout, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0x300, 256);

    MakeTestBuffer("b", 0x210, Vec4f(5.0f, 6.0f, 7.0f, 8.0f));
    FillDescriptor(singlesetlayout, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0x200, 512);

    MakeTestBuffer("c", 0x110, Vec4f(9.0f, 10.0f, 11.0f, 12.0f));
    FillDescriptor(singlesetlayout, 11, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0x100, 256,
                   VK_FORMAT_R32G32B32A32_SFLOAT);
    MakeTestBuffer("d", 0x410, Vec4f(13.0f, 14.0f, 15.0f, 16.0f));
    FillDescriptor(singlesetlayout, 12, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0x400, 512,
                   VK_FORMAT_R32G32B32A32_SFLOAT);

    FillDescriptor(singlesetlayout, 21, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_NULL_HANDLE, e);
    FillDescriptor(singlesetlayout, 22, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, f);

    FillDescriptor(singlesetlayout, 23, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_NULL_HANDLE, g);

    FillDescriptor(singlesetlayout, 31, VK_DESCRIPTOR_TYPE_SAMPLER, h, VK_NULL_HANDLE);

    FillDescriptor(singlesetlayout, 41, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, i_samp, i_tex);

    if(rays)
    {
      FillDescriptor(singlesetlayout, 51, j);
    }

    VkDeviceSize inlineOffset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, singlesetlayout, 61, &inlineOffset);
    Vec4f inlineData = Vec4f(17.0f, 18.0f, 19.0f, 20.0f);
    memcpy(descWrite + setOffset + inlineOffset + sizeof(Vec4f), &inlineData, sizeof(inlineData));

    ////////////// set 3 ////////////////
    ////////////// array ////////////////
    setOffset = setOffsets[3];

    MakeTestBuffer("m_20", 0x610, Vec4f(21.0f, 22.0f, 23.0f, 24.0f));
    FillDescriptor(arraysetlayout, {1, 20}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0x600, 256);
    FillDescriptor(arraysetlayout, {1, 31}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    FillDescriptor(arraysetlayout, {2, 20}, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n_20_samp,
                   n_20_tex);
    FillDescriptor(arraysetlayout, {2, 31}, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n_31_samp,
                   VK_NULL_HANDLE);
    FillDescriptor(arraysetlayout, {2, 41}, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n_41_samp,
                   VK_NULL_HANDLE);

    if(rays)
    {
      FillDescriptor(arraysetlayout, {3, 20}, u_20);
      FillDescriptor(arraysetlayout, {3, 31}, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    }

    MakeTestBuffer("o_40", 0xf10, Vec4f(25.0f, 26.0f, 27.0f, 28.0f));
    FillDescriptor(arraysetlayout, {4, 40}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0xf00, 256);
    FillDescriptor(arraysetlayout, {4, 51}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    //////////////  set 2  ////////////////
    ////////////// mutable ////////////////
    setOffset = setOffsets[2];
    mutableSet = true;

    FillDescriptor(mutablelayout, {0, 10}, VK_DESCRIPTOR_TYPE_SAMPLER, t_samp_10, VK_NULL_HANDLE);
    FillDescriptor(mutablelayout, {0, 20}, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_NULL_HANDLE,
                   t_tex_20);
    FillDescriptor(mutablelayout, {0, 30}, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   t_comb_30_samp, t_comb_30_tex);
    MakeTestBuffer("t_ubo_40", 0x510, Vec4f(29.0f, 30.0f, 31.0f, 32.0f));
    FillDescriptor(mutablelayout, {0, 40}, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0x500, 768);

    MakeTestBuffer("t_ssbo_50", 0x710, Vec4f(33.0f, 34.0f, 35.0f, 36.0f));
    FillDescriptor(mutablelayout, {0, 50}, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0x700, 256);

    if(rays)
    {
      FillDescriptor(mutablelayout, {0, 60}, t_as_60);
    }

    mutableSet = false;

    //////////////  set 1  ////////////////
    ////////////// sampler ///////////////
    setOffset = setOffsets[1];
    if(sampbuf.address)
    {
      descWrite = sampbuf.map();
      memset(descWrite, 0xcc, 0x100000);
    }

    FillDescriptor(samplayout, 0, VK_DESCRIPTOR_TYPE_SAMPLER, l, VK_NULL_HANDLE);

    // set 4 is push data, and set 5 is immutable samplers

    AllocatedBuffer pushbuf1 = MakeTestBuffer("p", 0x210, Vec4f(100.0f, 101.0f, 102.0f, 103.0f));
    AllocatedBuffer pushbuf2 = MakeTestBuffer("q", 0x310, Vec4f(104.0f, 105.0f, 106.0f, 107.0f));

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg;

      // normal calls
      {
        vkCmdBindDescriptorBuffersEXT(cmd, numBufs, descBind);

        // if we have a push buffer bind them all, if not bind starting from 1
        uint32_t numSets = 4;
        if(pushbuf.address)
          numSets++;

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, numSets,
                                           bufIdxs, setOffsets);
        vkCmdBindDescriptorBufferEmbeddedSamplersEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 5);

        std::vector<VkDescriptorBufferInfo> pushBufInfos = {
            vkh::DescriptorBufferInfo(pushbuf1.buffer, 0x200, 0x100)};
        vkCmdPushDescriptorSetKHR(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 4, 1,
            vkh::WriteDescriptorSet(VK_NULL_HANDLE, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    pushBufInfos));
        pushBufInfos = {vkh::DescriptorBufferInfo(pushbuf2.buffer, 0x300, 0x100)};
        vkCmdPushDescriptorSetKHR(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 4, 1,
            vkh::WriteDescriptorSet(VK_NULL_HANDLE, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    pushBufInfos));

        swapimg = StartUsingBackbuffer(cmd);

        vkh::cmdPipelineBarrier(
            cmd,
            {
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL, input.image),
            });

        vkh::cmdClearImage(cmd, input.image, vkh::ClearColorValue(1.0f, 0.5f, 0.0f, 1.0f));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, input.image),
                 });

        vkCmdBeginRenderPass(cmd,
                             vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                                      {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f)}),
                             VK_SUBPASS_CONTENTS_INLINE);

        mainWindow->setViewScissor(cmd);

        float sqSize = float(screenHeight) / ceilf(sqrtf((float)tests.size()));

        float x = 0.0f, y = 0.0f;

        for(size_t t = 0; t < tests.size(); t++)
        {
          VkViewport v = {x, y, sqSize, sqSize, 0.0f, 1.0f};
          vkh::cmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                                Vec4f(sqSize, (float)texSize, x, y));
          vkCmdSetViewport(cmd, 0, 1, &v);
          setMarker(cmd, "Normal Test " + std::to_string(t));
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tests[t]);
          vkCmdDraw(cmd, 4, 1, 0, 0);

          x += sqSize;

          if(x + sqSize >= (float)screenWidth)
          {
            x = 0.0f;
            y += sqSize;
          }
        }

        vkCmdEndRenderPass(cmd);
      }
      vkEndCommandBuffer(cmd);

      Submit(0, 2, {cmd});

      cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      // maint 6 calls
      {
        vkCmdBindDescriptorBuffersEXT(cmd, numBufs, descBind);

        // if we have a push buffer bind them all, if not bind starting from 1
        uint32_t numSets = 4;
        if(pushbuf.address)
          numSets++;

        VkSetDescriptorBufferOffsetsInfoEXT setInfo = {
            VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
        };

        // user could cover multiple pipeline layouts, ensure that works
        // even if we don't specify fragment bit this still counts as covering all graphics stages
        setInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        setInfo.layout = layout;
        setInfo.firstSet = 0;
        setInfo.setCount = numSets;
        setInfo.pBufferIndices = bufIdxs;
        setInfo.pOffsets = setOffsets;

        vkCmdSetDescriptorBufferOffsets2EXT(cmd, &setInfo);

        VkBindDescriptorBufferEmbeddedSamplersInfoEXT embedInfo = {
            VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT,
        };

        embedInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        embedInfo.layout = layout;
        embedInfo.set = 5;

        vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(cmd, &embedInfo);

        VkPushDescriptorSetInfo pushInfo = {
            VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_INFO,
        };

        VkWriteDescriptorSet write;

        pushInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        pushInfo.descriptorWriteCount = 1;
        pushInfo.pDescriptorWrites = &write;
        pushInfo.layout = layout;
        pushInfo.set = 4;

        std::vector<VkDescriptorBufferInfo> pushBufInfos = {
            vkh::DescriptorBufferInfo(pushbuf1.buffer, 0x200, 0x100)};
        write = vkh::WriteDescriptorSet(VK_NULL_HANDLE, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        pushBufInfos);

        vkCmdPushDescriptorSet2KHR(cmd, &pushInfo);
        pushBufInfos = {vkh::DescriptorBufferInfo(pushbuf2.buffer, 0x300, 0x100)};
        write = vkh::WriteDescriptorSet(VK_NULL_HANDLE, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        pushBufInfos);
        vkCmdPushDescriptorSet2KHR(cmd, &pushInfo);

        vkCmdBeginRenderPass(cmd,
                             vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                                      {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f)}),
                             VK_SUBPASS_CONTENTS_INLINE);

        mainWindow->setViewScissor(cmd);

        float sqSize = float(screenHeight) / ceilf(sqrtf((float)tests.size()));

        float x = 0.0f, y = 0.0f;

        for(size_t t = 0; t < tests.size(); t++)
        {
          VkViewport v = {x, y, sqSize, sqSize, 0.0f, 1.0f};
          vkh::cmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                                Vec4f(sqSize, (float)texSize, x, y));
          vkCmdSetViewport(cmd, 0, 1, &v);
          setMarker(cmd, "Maint6 Test " + std::to_string(t));
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tests[t]);
          vkCmdDraw(cmd, 4, 1, 0, 0);

          x += sqSize;

          if(x + sqSize >= (float)screenWidth)
          {
            x = 0.0f;
            y += sqSize;
          }
        }

        vkCmdEndRenderPass(cmd);
      }

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                           VK_IMAGE_LAYOUT_GENERAL, colatt.image),
               });

      blitToSwap(cmd, colatt.image, VK_IMAGE_LAYOUT_GENERAL, swapimg, VK_IMAGE_LAYOUT_GENERAL);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(1, 2, {cmd});
      Present();
    }

    vkDestroyAccelerationStructureKHR(device, j, NULL);
    vkDestroyAccelerationStructureKHR(device, t_as_60, NULL);
    vkDestroyAccelerationStructureKHR(device, u_20, NULL);
    vkDestroyAccelerationStructureKHR(device, blas, NULL);
    descbuf.unmap();
    if(sampbuf.address)
      sampbuf.unmap();

    return 0;
  }
};

REGISTER_TEST();
