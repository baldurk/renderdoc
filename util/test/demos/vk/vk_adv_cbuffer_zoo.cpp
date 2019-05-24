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

struct vec2
{
  float x, y;
};
struct vec3
{
  float x, y, z;
};
struct i8vec4
{
  int8_t x, y, z, w;
};
struct i8vec2
{
  int8_t x, y;
};
struct i16vec4
{
  int16_t x, y, z, w;
};
struct i16vec3
{
  int16_t x, y, z;
};
struct i16vec2
{
  int16_t x, y;
};
struct mat2x3
{
  float m[2 * 3];
};

// Block memory layout
struct S
{
  float a;
  vec2 b;
  double c;
  float d;
  vec3 e;
  float f;
};

struct S8
{
  int8_t a;
  i8vec4 b;
  i8vec2 c[4];
};

struct S16
{
  uint16_t a;
  i16vec4 b;
  i16vec2 c[4];
  int8_t d;
};

struct UBO
{
  float a;
  vec2 b;
  vec3 c;
  float d[2];
  mat2x3 e;
  mat2x3 f[2];
  float g;
  S h;
  S i[2];
  // i8vec4 pad1;
  int8_t j;
  S8 k;
  S8 l[2];
  int8_t m;
  S16 n;
  uint8_t o;
  S16 p[2];
  uint64_t q;
  int64_t r;
  uint16_t s;
  int8_t test;
};

TEST(VK_Adv_CBuffer_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests VK_EXT_scalar_block_layout as well as 8-bit/16-bit storage "
      "to ensure we correctly handle all types of offset and type.";

  std::string common = R"EOSHADER(

#version 460 core

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

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

// Block memory layout
struct S
{
    float      a;   // offset 0
    vec2       b;   // offset 4
    double     c;   // offset 16
    float      d;   // offset 24
    vec3       e;   // offset 28
    float      f;   // offset 40
    // size = 44, align = 8
};

struct S8
{
    int8_t     a;     // offset 0
    i8vec4     b;     // offset 1
    i8vec2     c[4];  // offset 5
    // size = 13, align = 1
};

struct S16
{
    uint16_t    a;     // offset 0
    i16vec4     b;     // offset 2
    i16vec2     c[4];  // offset 10
    int8_t      d;     // offset 26
    // size = 27, align = 2
};

layout(column_major, scalar) uniform B1
{
    float      a;     // offset = 0
    vec2       b;     // offset = 4
    vec3       c;     // offset = 12
    float      d[2];  // offset = 24
    mat2x3     e;     // offset = 32, takes 24 bytes, matrixstride = 12
    mat2x3     f[2];  // offset = 56, takes 48 bytes, matrixstride = 12, arraystride = 24
    float      g;     // offset = 104
    S          h;     // offset = 112 (aligned to multiple of 8)
    S          i[2];  // offset = 160 (aligned to multiple of 8) stride = 48
    i8vec4     pad1;  // offset = 252 C pads after array here - not required in GLSL scalar packing
    int8_t     j;     // offset = 256
    S8         k;     // offset = 257 (aligned to multiple of 1)
    S8         l[2];  // offset = 270 (aligned to multiple of 1) stride = 13
    int8_t     m;     // offset = 296
    S16        n;     // offset = 298 (aligned to multiple of 2)
    int8_t     pad2;  // offset = 325 C pads after struct here - not required in GLSL scalar packing
    uint8_t    o;     // offset = 326
    S16        p[2];  // offset = 328 (aligned to multiple of 2) stride = 28
    int8_t     pad3;  // offset = 383 C pads after struct here - not required in GLSL scalar packing
    uint64_t   q;     // offset = 384
    int64_t    r;     // offset = 392
    float16_t  s;     // offset = 400
    int8_t     test;  // offset = 402
};

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.uv = vec4(UV.xy, 0, 1);

  vertOut.col = vec4(1,0,0,0);

  if(int(test) == 42)
    vertOut.col = vec4(0,1,0,0);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
    devExts.push_back(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);
    devExts.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    devExts.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);

    features.shaderFloat64 = true;
    features.shaderInt64 = true;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDevice16BitStorageFeaturesKHR _16bitFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,
    };

    static VkPhysicalDevice16BitStorageFeaturesKHR _8bitFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
    };

    static VkPhysicalDeviceScalarBlockLayoutFeaturesEXT scalarFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,
    };

    getPhysFeatures2(&_16bitFeatures);
    getPhysFeatures2(&_8bitFeatures);
    getPhysFeatures2(&scalarFeatures);

    if(!scalarFeatures.scalarBlockLayout)
      Avail = "Scalar block layout feature 'scalarBlockLayout' not available";
    else if(!_8bitFeatures.uniformAndStorageBuffer16BitAccess)
      Avail = "8-bit storage feature 'uniformAndStorageBuffer16BitAccess' not available";
    else if(!_16bitFeatures.uniformAndStorageBuffer16BitAccess)
      Avail = "16-bit storage feature 'uniformAndStorageBuffer16BitAccess' not available";

    devInfoNext = &_8bitFeatures;
    _8bitFeatures.pNext = &_16bitFeatures;
    _16bitFeatures.pNext = &scalarFeatures;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
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

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    UBO cbufferdata = {};

    // this value is checked by the shader to ensure everything has aligned just so.
    cbufferdata.test = 42;

    // set some other values that we can inspect ourselves manually
    cbufferdata.a = 1.0f;
    cbufferdata.b.x = 2.0f;
    cbufferdata.c.y = 3.0f;
    cbufferdata.d[0] = 4.0f;
    cbufferdata.d[1] = 5.0f;
    cbufferdata.e.m[0] = 6.0f;
    cbufferdata.e.m[1] = 7.0f;
    cbufferdata.e.m[3] = 999.0f;
    cbufferdata.f[0].m[0] = 8.0f;
    cbufferdata.f[0].m[1] = 9.0f;
    cbufferdata.f[0].m[3] = 999.0f;
    cbufferdata.f[1].m[0] = 10.0f;
    cbufferdata.f[1].m[1] = 11.0f;
    cbufferdata.f[1].m[3] = 999.0f;
    cbufferdata.g = 12.0f;
    cbufferdata.h.c = 13.0;
    cbufferdata.h.d = 14.0f;
    cbufferdata.i[0].c = 15.0;
    cbufferdata.i[1].d = 16.0f;
    cbufferdata.j = 17;
    cbufferdata.k.c[1].y = 18;
    cbufferdata.l[0].a = 19;
    cbufferdata.l[0].c[1].y = 20;
    cbufferdata.l[1].a = 21;
    cbufferdata.l[1].c[0].y = 22;
    cbufferdata.m = -23;
    cbufferdata.n.a = 65524;
    cbufferdata.n.b.w = -2424;
    cbufferdata.n.d = 25;
    cbufferdata.o = 226;
    cbufferdata.p[0].b.z = 2727;
    cbufferdata.p[0].d = 28;
    cbufferdata.p[1].b.w = 2929;
    cbufferdata.q = 30303030303030ULL;
    cbufferdata.r = -31313131313131LL;
    cbufferdata.s = 19472;    // 16.25f

    AllocatedBuffer cb(
        allocator, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(&cbufferdata, sizeof(cbufferdata));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(cb.buffer)}),
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

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor,
                                        {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)}),
          VK_SUBPASS_CONTENTS_INLINE);

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();