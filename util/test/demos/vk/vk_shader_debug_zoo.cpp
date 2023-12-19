/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <limits>
#include "3rdparty/fmt/core.h"
#include "vk_test.h"

RD_TEST(VK_Shader_Debug_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests shader debugging on SPIR-V opcodes.";

  struct ConstsA2V
  {
    Vec4f pos;
    float zero;
    float one;
    float negone;
    Vec2f uv;
  };

  std::string v2f =
      R"EOSHADER(

struct flatv2f
{
  uint test;
  uint intval;
};

struct v2f
{
  vec2 zeroVal;
  vec2 inpos;
  vec2 inposIncreased;
  float tinyVal;
  float oneVal;
  float negoneVal;
};

layout(location = 1) inout_type flat flatv2f flatData;
layout(location = 3) inout_type v2f linearData;

)EOSHADER";

  std::string vertex = R"EOSHADER(
#version 430 core

#define inout_type out

)EOSHADER" + v2f + R"EOSHADER(

layout(location = 0) in vec4 pos;
layout(location = 1) in float zero;
layout(location = 2) in float one;
layout(location = 3) in float negone;

void main()
{
  int test = gl_InstanceIndex;
 
  gl_Position = vec4(pos.x + pos.z * float(test % 256), pos.y + pos.w * float(test / 256), 0.0, 1.0);

  const vec4 verts[4] = vec4[4](vec4(-1.0, -1.0, 0.5, 1.0), vec4(1.0, -1.0, 0.5, 1.0),
                                vec4(-1.0, 1.0, 0.5, 1.0), vec4(1.0, 1.0, 0.5, 1.0));

  const vec2 data[3] = vec2[3](vec2(10.0f, 10.0f), vec2(20.0f, 10.0f), vec2(10.0f, 20.0f));

  linearData.zeroVal = zero.xx;
  linearData.oneVal = one;
  linearData.negoneVal = negone;
  linearData.tinyVal = one * 1.0e-30;
  linearData.inpos = data[gl_VertexIndex];
  linearData.inposIncreased = data[gl_VertexIndex] * 2.75f;
  flatData.test = test;
  flatData.intval = test + 7;
}

)EOSHADER";

  std::string pixel_glsl_header = R"EOSHADER(
#version 460 core

#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_nonuniform_qualifier : require

#define TEST_DESC_INDEXING  

layout(set = 0, binding = 10, std140) uniform constsbuf
{
  vec4 first;
  uint uniformIndex;
  vec4 second;
  vec4 nan;
  vec4 third;
  vec4 pad3;
  vec4 fourth;
  vec4 unorm2PackSource;
  vec4 snorm2PackSource;
  vec4 unorm4PackSource;
  vec4 snorm4PackSource;
  vec4 halfPackSource;
  uint unormUnpackSource;
  uint snormUnpackSource;
  uint halfUnpackSource;
  uint pad;
} cbuf;

layout(set = 0, binding = 11) uniform sampler pointSampler;
layout(set = 0, binding = 12) uniform sampler linearSampler;

layout(set = 0, binding = 13) uniform texture2D sampledImage;

layout(set = 0, binding = 14) uniform sampler2D linearSampledImage;

struct dummy
{
  uvec4 val;
  uvec4 val2;
};

layout(set = 0, binding = 15, std430) buffer storebuftype
{
  layout(row_major) mat4 a;
  layout(column_major) mat4 b;
  vec4 x;
  dummy y;
  vec4 arr[];
} storebuf;

layout(set = 0, binding = 16, rgba32f) uniform coherent image2D storeImage;

layout(set = 0, binding = 17) uniform samplerBuffer texBuffer;
layout(set = 0, binding = 18, rgba32f) uniform coherent imageBuffer storeTexBuffer;

layout(set = 0, binding = 19) uniform sampler shadowSampler;

layout(set = 0, binding = 20) uniform samplerCube cubeSampler;

layout(set = 0, binding = 21, std430) buffer atomicbuftype
{
  uvec4 data[];
} atomicbuf;

layout(set = 0, r32ui, binding = 22) uniform uimage2D atomicimg;

layout(set = 0, binding = 30) uniform sampler2DArray queryTest;
layout(set = 0, binding = 31) uniform sampler2DMSArray queryTestMS;

layout(set = 0, binding = 32) uniform texture2D depthImage;

#if TEST_DESC_INDEXING

layout(set = 1, binding = 1) uniform sampler pointSamplers[14];
layout(set = 1, binding = 2) uniform sampler linearSamplers[14];

layout(set = 1, binding = 3) uniform texture2D sampledImages[14];

layout(set = 1, binding = 4) uniform sampler2D linearSampledImages[14];

layout(set = 1, binding = 5, std430) buffer storebufstype
{
  vec4 x;
  dummy y;
  vec4 arr[];
} storebufs[14];

layout(set = 1, binding = 6, rgba32f) uniform coherent image2D storeImages[14];

layout(set = 1, binding = 7) uniform samplerBuffer texBuffers[14];
layout(set = 1, binding = 8, rgba32f) uniform coherent imageBuffer storeTexBuffers[14];

layout(set = 1, binding = 9) uniform sampler shadowSamplers[14];

layout(set = 1, binding = 20) uniform sampler2DArray queryTests[14];
layout(set = 1, binding = 21) uniform sampler2DMSArray queryTestsMS[14];

layout(set = 2, binding = 0) uniform sampler1D zoo_1D;
layout(set = 2, binding = 1) uniform sampler2D zoo_2D;
layout(set = 2, binding = 2) uniform sampler3D zoo_3D;
layout(set = 2, binding = 3) uniform samplerCube zoo_Cube;
layout(set = 2, binding = 4) uniform sampler1DArray zoo_1DArray;
layout(set = 2, binding = 5) uniform sampler2DArray zoo_2DArray;
layout(set = 2, binding = 6) uniform samplerCubeArray zoo_CubeArray;
layout(set = 2, binding = 7) uniform sampler2DMS zoo_2DMS;
layout(set = 2, binding = 8) uniform sampler2DMSArray zoo_2DMSArray;
layout(set = 2, binding = 9) uniform samplerBuffer zoo_Buffer;

layout(set = 2, binding = 10) uniform usampler1D zoo_u1D;
layout(set = 2, binding = 11) uniform usampler2D zoo_u2D;
layout(set = 2, binding = 12) uniform usampler3D zoo_u3D;
layout(set = 2, binding = 13) uniform usamplerCube zoo_uCube;
layout(set = 2, binding = 14) uniform usampler1DArray zoo_u1DArray;
layout(set = 2, binding = 15) uniform usampler2DArray zoo_u2DArray;
layout(set = 2, binding = 16) uniform usamplerCubeArray zoo_uCubeArray;
layout(set = 2, binding = 17) uniform usampler2DMS zoo_u2DMS;
layout(set = 2, binding = 18) uniform usampler2DMSArray zoo_u2DMSArray;
layout(set = 2, binding = 19) uniform usamplerBuffer zoo_uBuffer;

layout(set = 2, binding = 20) uniform isampler1D zoo_i1D;
layout(set = 2, binding = 21) uniform isampler2D zoo_i2D;
layout(set = 2, binding = 22) uniform isampler3D zoo_i3D;
layout(set = 2, binding = 23) uniform isamplerCube zoo_iCube;
layout(set = 2, binding = 24) uniform isampler1DArray zoo_i1DArray;
layout(set = 2, binding = 25) uniform isampler2DArray zoo_i2DArray;
layout(set = 2, binding = 26) uniform isamplerCubeArray zoo_iCubeArray;
layout(set = 2, binding = 27) uniform isampler2DMS zoo_i2DMS;
layout(set = 2, binding = 28) uniform isampler2DMSArray zoo_i2DMSArray;
layout(set = 2, binding = 29) uniform isamplerBuffer zoo_iBuffer;

layout(set = 2, rgba32f, binding = 30) uniform image1D storezoo_1D;
layout(set = 2, rgba32f, binding = 31) uniform image2D storezoo_2D;
layout(set = 2, rgba32f, binding = 32) uniform image3D storezoo_3D;
layout(set = 2, rgba32f, binding = 33) uniform imageCube storezoo_Cube;
layout(set = 2, rgba32f, binding = 34) uniform image1DArray storezoo_1DArray;
layout(set = 2, rgba32f, binding = 35) uniform image2DArray storezoo_2DArray;
layout(set = 2, rgba32f, binding = 36) uniform imageCubeArray storezoo_CubeArray;
//layout(set = 2, rgba32f, binding = 37) uniform image2DMS storezoo_2DMS;
//layout(set = 2, rgba32f, binding = 38) uniform image2DMSArray storezoo_2DMSArray;
layout(set = 2, rgba32f, binding = 39) uniform imageBuffer storezoo_Buffer;

layout(set = 2, rgba32ui, binding = 40) uniform uimage1D storezoo_u1D;
layout(set = 2, rgba32ui, binding = 41) uniform uimage2D storezoo_u2D;
layout(set = 2, rgba32ui, binding = 42) uniform uimage3D storezoo_u3D;
layout(set = 2, rgba32ui, binding = 43) uniform uimageCube storezoo_uCube;
layout(set = 2, rgba32ui, binding = 44) uniform uimage1DArray storezoo_u1DArray;
layout(set = 2, rgba32ui, binding = 45) uniform uimage2DArray storezoo_u2DArray;
layout(set = 2, rgba32ui, binding = 46) uniform uimageCubeArray storezoo_uCubeArray;
//layout(set = 2, rgba32ui, binding = 47) uniform uimage2DMS storezoo_u2DMS;
//layout(set = 2, rgba32ui, binding = 48) uniform uimage2DMSArray storezoo_u2DMSArray;
layout(set = 2, rgba32ui, binding = 49) uniform uimageBuffer storezoo_uBuffer;

layout(set = 2, rgba32i, binding = 50) uniform iimage1D storezoo_i1D;
layout(set = 2, rgba32i, binding = 51) uniform iimage2D storezoo_i2D;
layout(set = 2, rgba32i, binding = 52) uniform iimage3D storezoo_i3D;
layout(set = 2, rgba32i, binding = 53) uniform iimageCube storezoo_iCube;
layout(set = 2, rgba32i, binding = 54) uniform iimage1DArray storezoo_i1DArray;
layout(set = 2, rgba32i, binding = 55) uniform iimage2DArray storezoo_i2DArray;
layout(set = 2, rgba32i, binding = 56) uniform iimageCubeArray storezoo_iCubeArray;
//layout(set = 2, rgba32i, binding = 57) uniform iimage2DMS storezoo_i2DMS;
//layout(set = 2, rgba32i, binding = 58) uniform iimage2DMSArray storezoo_i2DMSArray;
layout(set = 2, rgba32i, binding = 59) uniform iimageBuffer storezoo_iBuffer;

#endif

layout(push_constant) uniform PushData {
  layout(offset = 16) ivec4 data;
} push;
)EOSHADER";

  std::string pixel_glsl1 = pixel_glsl_header + R"EOSHADER(

layout(location = 0, index = 0) out vec4 Color;

#define inout_type in

)EOSHADER" + v2f +
                            R"EOSHADER(

vec4 varscope_test(int coord, vec2 inpos_param, vec2 inpos_incr_param)
{
  float never_in_scope;

  if(coord < 0)
  {
    never_in_scope = inpos_param.x;
    never_in_scope *= 2.0f;
  }

  vec4 ret;

  // for the first pixel ret comes into scope early
  if(coord == 0)
  {
    ret = vec4(0.5, 0.5, 0.5, 0.0);
  }

  float long_scope;

  {
    float short_scope;
    short_scope = inpos_param.y;
    short_scope = sin(short_scope);
    long_scope = short_scope * inpos_incr_param.x;
  }

  if(coord != 0)
  {
    ret = vec4(1.0, 1.0, 1.0, 0.0);
  }

  ret.w += long_scope;

  ret *= 1.5f;

  return ret;
}

void main()
{
  float  posinf = linearData.oneVal/linearData.zeroVal.x;
  float  neginf = linearData.negoneVal/linearData.zeroVal.x;
  float  nan = linearData.zeroVal.x/linearData.zeroVal.y;
  nan *= cbuf.nan.x;

  float negone = linearData.negoneVal;
  float posone = linearData.oneVal;
  float zerof = linearData.zeroVal.x;
  float tiny = linearData.tinyVal;

  int intval = int(flatData.intval);
  uint zerou = flatData.intval - flatData.test - 7u;
  int zeroi = int(zerou);

  uint test = flatData.test;

  vec2 inpos = linearData.inpos;
  vec2 inposIncreased = linearData.inposIncreased;

  ivec2 localCoord = ivec2(gl_FragCoord) % ivec2(4, 4);
  int flatLocalCoord = localCoord.x + localCoord.y * 4;

  int flatGlobalCoord = int(gl_FragCoord.x) + int(gl_FragCoord.y) * 1024;

  Color = vec4(0,0,0,0);
  switch(test)
  {
    case 0:
    {
      Color = gl_FragCoord;
      break;
    }
    case 1:
    {
      Color = dFdx(gl_FragCoord);
      break;
    }
    case 2:
    {
      Color = dFdy(gl_FragCoord);
      break;
    }
    case 3:
    {
      Color = dFdxCoarse(gl_FragCoord);
      break;
    }
    case 4:
    {
      Color = dFdyCoarse(gl_FragCoord);
      break;
    }
    case 5:
    {
      Color = dFdxFine(gl_FragCoord);
      break;
    }
    case 6:
    {
      Color = dFdyFine(gl_FragCoord);
      break;
    }
    case 7:
    {
      Color = dFdx(vec4(inpos, inposIncreased));
      break;
    }
    case 8:
    {
      Color = dFdy(vec4(inpos, inposIncreased));
      break;
    }
    case 9:
    {
      Color = dFdxCoarse(vec4(inpos, inposIncreased));
      break;
    }
    case 10:
    {
      Color = dFdyCoarse(vec4(inpos, inposIncreased));
      break;
    }
    case 11:
    {
      Color = dFdxFine(vec4(inpos, inposIncreased));
      break;
    }
    case 12:
    {
      Color = dFdyFine(vec4(inpos, inposIncreased));
      break;
    }
    case 13:
    {
      Color = vec4(abs(posone*2.5f), abs(negone*2.5f), abs(zerof*2.5f), 1.0f);
      break;
    }
    case 14:
    {
      Color = vec4(pow(posone*2.5f, posone*1.3f), pow(posone*2.5f, posone*0.45f),
                   pow(vec2(posone*2.5f, posone*1.3f), vec2(posone*0.9f, posone*8.5f)));
      break;
    }
    case 15:
    {
      Color = vec4(normalize(posone*2.5f), normalize(posone), normalize(negone), 1.0f);
      break;
    }
    case 16:
    {
      Color = vec4(normalize(vec2(posone*2.5f, negone*1.8f)), normalize(vec2(posone*8.5f, negone*7.1f)));
      break;
    }
    case 17:
    {
      Color = vec4(normalize(vec3(posone*2.5f, negone*1.8f, posone*8.5f)), 1.0f);
      break;
    }
    case 18:
    {
      Color = normalize(vec4(posone*2.5f, negone*1.8f, posone*8.5f, negone*5.2f));
      break;
    }
    case 19:
    {
      Color = vec4(floor(posone*2.5f), floor(posone*2.4f), floor(posone*2.6f), floor(zerof));
      break;
    }
    case 20:
    {
      Color = vec4(floor(negone*2.5f), floor(negone*2.4f), floor(negone*2.6f), 1.0f);
      break;
    }
    case 21:
    {
      Color = vec4(mix(posone*1.1f, posone*3.3f, 0.5f),
                   mix(posone*1.1f, posone*3.3f, 0.2f),
                   mix(posone*1.1f, posone*3.3f, 0.8f),
                   1.0f);
      break;
    }
    case 22:
    {
      Color = vec4(mix(posone*1.1f, posone*3.3f, 1.5f),
                   mix(posone*1.1f, posone*3.3f, -0.3f),
                   0.0f,
                   1.0f);
      break;
    }
    case 23:
    {
      Color = vec4(mix(posone*3.3f, posone*1.1f, 0.5f),
                   mix(posone*3.3f, posone*1.1f, 0.2f),
                   mix(posone*3.3f, posone*1.1f, 0.8f),
                   1.0f);
      break;
    }
    case 24:
    {
      vec3 a = vec3(posone*2.5f, negone*1.8f, posone*8.5f);
      vec3 b = vec3(negone*6.3f, posone*3.2f, negone*0.4f);
      Color = vec4(cross(a, b), 1.0f);
      break;
    }
    case 25:
    {
      vec4 a = vec4(posone*2.5f, negone*1.8f, posone*8.5f, posone*3.9f);
      vec4 b = vec4(negone*6.3f, posone*3.2f, negone*0.4f, zerof);
      Color = vec4(dot(a.xyz, b.xyz), dot(a.w, b.w), dot(a, b), dot(a.wz, b.ww));
      break;
    }
    case 26:
    {
      Color = cbuf.first;
      break;
    }
    case 27:
    {
      Color = cbuf.second;
      break;
    }
    case 28:
    {
      Color = cbuf.third;
      break;
    }
    case 29:
    {
      Color = cbuf.fourth;
      break;
    }
    case 30:
    {
      Color = cbuf.first + cbuf.second + cbuf.third + cbuf.fourth +
              cbuf.pad3;
      break;
    }
    case 31:
    {
      ivec2 coord = ivec2(zeroi + 20, zeroi + 20);

      Color = texelFetch(sampledImage, coord, 0);
      break;
    }
    case 32:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImage, pointSampler), coord, 0.0);
      break;
    }
    case 33:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImage, linearSampler), coord, 0.0);
      break;
    }
    case 34:
    {
      Color = texture(linearSampledImage, inpos);
      break;
    }
    case 35:
    {
      Color = vec4(max(posone*3.3f, posone*4.4f),
                   max(posone*4.4f, posone*3.3f),
                   max(posone, posinf),
                   max(posone, neginf));
      break;
    }
    case 36:
    {
      Color = vec4(max(negone*3.3f, negone*4.4f),
                   max(negone*4.4f, negone*3.3f),
                   max(negone, posinf),
                   max(negone, neginf));
      break;
    }
    case 37:
    {
      Color = vec4(min(posone*3.3f, posone*4.4f),
                   min(posone*4.4f, posone*3.3f),
                   min(posone, posinf),
                   min(posone, neginf));
      break;
    }
    case 38:
    {
      Color = vec4(min(negone*3.3f, negone*4.4f),
                   min(negone*4.4f, negone*3.3f),
                   min(negone, posinf),
                   min(negone, neginf));
      break;
    }
    case 39:
    {
      Color = vec4(float(max(zeroi+5, zeroi+8)),
                   float(max(zeroi+8, zeroi+5)),
                   float(max(zeroi-8, zeroi-5)),
                   float(max(zeroi-5, zeroi-8)));
      break;
    }
    case 40:
    {
      Color = vec4(float(min(zeroi+5, zeroi+8)),
                   float(min(zeroi+8, zeroi+5)),
                   float(min(zeroi-8, zeroi-5)),
                   float(min(zeroi-5, zeroi-8)));
      break;
    }
    case 41:
    {
      Color = vec4(float(max(zerou+5, zerou+8)),
                   float(max(zerou+8, zerou+5)),
                   float(min(zerou+8, zerou+5)),
                   float(min(zerou+5, zerou+8)));
      break;
    }
    case 42:
    {
      Color = vec4(clamp(posone*3.3f, posone, posone*5.0f),
                   clamp(posone*0.3f, posone, posone*5.0f),
                   clamp(posone*8.3f, posone, posone*5.0f),
                   1.0f);
      break;
    }
    case 43:
    {
      uint x = uint(posone);
      Color = vec4(float(clamp(x*4, zerou+2, zerou+50)),
                   float(clamp(x, zerou+2, zerou+50)),
                   float(clamp(x*400, zerou+2, zerou+50)),
                   1.0f);
      break;
    }
    case 44:
    {
      int x = int(posone);
      Color = vec4(float(clamp(x*4, zeroi+2, zeroi+50)),
                   float(clamp(x, zeroi+2, zeroi+50)),
                   float(clamp(x*400, zeroi+2, zeroi+50)),
                   1.0f);
      break;
    }
    case 45:
    {
      Color = vec4(float(abs(zeroi+2)),
                   float(abs(zeroi)),
                   float(abs(zeroi-5)),
                   1.0f);
      break;
    }
    case 46:
    {
      Color = fwidth(gl_FragCoord);
      break;
    }
    case 47:
    {
      Color = fwidthCoarse(gl_FragCoord);
      break;
    }
    case 48:
    {
      Color = fwidthFine(gl_FragCoord);
      break;
    }
    case 49:
    {
      Color = fwidth(vec4(inpos, inposIncreased));
      break;
    }
    case 50:
    {
      Color = fwidthCoarse(vec4(inpos, inposIncreased));
      break;
    }
)EOSHADER"
                            R"EOSHADER(
    case 51:
    {
      Color = fwidthFine(vec4(inpos, inposIncreased));
      break;
    }
    case 52:
    {
      Color = vec4(isinf(posone) ? 1.0f : 0.0f, isinf(zerof) ? 1.0f : 0.0f, isinf(negone) ? 1.0f : 0.0f, 1.0f);
      break;
    }
    case 53:
    {
      Color = vec4(isnan(posone) ? 1.0f : 0.0f, isnan(zerof) ? 1.0f : 0.0f, isnan(negone) ? 1.0f : 0.0f, 1.0f);
      break;
    }
    case 54:
    {
      Color = vec4(isinf(posinf) ? 1.0f : 0.0f, isinf(neginf) ? 1.0f : 0.0f, isinf(nan) ? 1.0f : 0.0f, 1.0f);
      break;
    }
    case 55:
    {
      Color = vec4(isnan(posinf) ? 1.0f : 0.0f, isnan(neginf) ? 1.0f : 0.0f, isnan(nan) ? 1.0f : 0.0f, 1.0f);
      break;
    }
    case 56:
    {
      Color = vec4(push.data);
      break;
    }
    case 57:
    {
      Color = vec4(roundEven(posone*2.5f), roundEven(posone*3.5f), roundEven(posone*4.5f), roundEven(posone*5.1f));
      break;
    }
    case 58:
    {
      Color = vec4(roundEven(negone*2.5f), roundEven(negone*3.5f), roundEven(negone*4.5f), roundEven(negone*5.1f));
      break;
    }
    case 59:
    {
      // avoid implementation-defined behaviour at half-way points
      Color = vec4(round(posone*2.4f), round(posone*3.6f), round(posone*4.6f), round(posone*5.1f));
      break;
    }
    case 60:
    {
      Color = vec4(round(negone*2.6f), round(negone*3.6f), round(negone*4.6f), round(posone*5.1f));
      break;
    }
    case 61:
    {
      Color = vec4(trunc(posone*2.4f), trunc(posone*2.5f), trunc(posone*2.6f), trunc(posone*5.1f));
      break;
    }
    case 62:
    {
      Color = vec4(trunc(negone*2.4f), trunc(negone*2.5f), trunc(negone*2.6f), trunc(negone*3.1f));
      break;
    }
    case 63:
    {
      Color = vec4(fract(posone*2.4f), fract(posone*2.5f), fract(posone*2.6f), fract(posone*3.1f));
      break;
    }
    case 64:
    {
      Color = vec4(fract(negone*2.4f), fract(negone*2.5f), fract(negone*2.6f), fract(negone*3.1f));
      break;
    }
    case 65:
    {
      Color = vec4(ceil(posone*2.4f), ceil(posone*2.5f), ceil(posone*2.6f), ceil(posone*3.1f));
      break;
    }
    case 66:
    {
      Color = vec4(ceil(negone*2.4f), ceil(negone*2.5f), ceil(negone*2.6f), ceil(negone*3.1f));
      break;
    }
    case 67:
    {
      Color = vec4(sign(negone*2.4f), sign(posone*2.4f), sign(posinf), sign(neginf));
      break;
    }
    case 68:
    {
      int onei = zeroi+1;
      int negi = zeroi-1;
      Color = vec4(float(sign(onei*2)), float(sign(negi*2)), float(sign(0)), 1.0f);
      break;
    }
    case 69:
    {
      Color = vec4(degrees(negone*2.4f), degrees(posone*2.4f), degrees(zerof), degrees(posone*34.56f));
      break;
    }
    case 70:
    {
      Color = vec4(radians(negone*164.2f), radians(posone*164.2f), radians(zerof), radians(posone*3456.78f));
      break;
    }
    case 71:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      Color = vec4(float(a.x < b.x), float(a.x <= b.x), float(a.x > b.x), float(a.x >= b.x));
      break;
    }
    case 72:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      bvec4 c = lessThanEqual(a, b);
      Color = vec4(float(a.x == b.x), float(a.x != b.x), 0.0f, 1.0f);
      break;
    }
    case 73:
    {
      ivec4 a = ivec4(zeroi+2, zeroi+3, zeroi+4, zeroi+5);
      ivec4 b = ivec4(zeroi+4, zeroi+4, zeroi+4, zeroi+4);
      Color = vec4(float(a.x < b.x), float(a.x <= b.x), float(a.x > b.x), float(a.x >= b.x));
      break;
    }
    case 74:
    {
      ivec4 a = ivec4(zeroi+2, zeroi+3, zeroi+4, zeroi+5);
      ivec4 b = ivec4(zeroi+4, zeroi+4, zeroi+4, zeroi+4);
      Color = vec4(float(a.x == b.x), float(a.x != b.x), 0.0f, 1.0f);
      break;
    }
    case 75:
    {
      uvec4 a = uvec4(zerou+2, zerou+3, zerou+4, zerou+5);
      uvec4 b = uvec4(zerou+4, zerou+4, zerou+4, zerou+4);
      Color = vec4(float(a.x < b.x), float(a.x <= b.x), float(a.x > b.x), float(a.x >= b.x));
      break;
    }
    case 76:
    {
      uvec4 a = uvec4(zerou+2, zerou+3, zerou+4, zerou+5);
      uvec4 b = uvec4(zerou+4, zerou+4, zerou+4, zerou+4);
      Color = vec4(float(a.x == b.x), float(a.x != b.x), 0.0f, 1.0f);
      break;
    }
    case 77:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      bvec4 c = lessThanEqual(a, b);
      Color = vec4(float(any(c)), float(all(c)), float(c.x == c.z), float(c.x != c.w));
      break;
    }
    case 78:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      bvec4 c = lessThanEqual(a, b);
      Color = vec4(float(c.x || c.y), float(c.x && c.y), float(!c.x), 1.0f);
      break;
    }
    case 79:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      bvec4 c = lessThanEqual(a, b);
      Color = mix(vec4(posone*9.0f, posone*8.0f, posone*7.0f, posone*6.0f),
                  vec4(posone*1.0f, posone*2.0f, posone*3.0f, posone*4.0f), c);
      break;
    }
    case 80:
    {
      discard;
    }
    case 81:
    {
      Color = vec4(sin(posone*2.4f), cos(posone*2.4f), asin(posone*2.4f), acos(posone*2.4f));
      break;
    }
    case 82:
    {
      Color = vec4(sinh(posone*2.4f), cosh(posone*2.4f), asinh(posone*2.4f), acosh(posone*2.4f));
      break;
    }
    case 83:
    {
      Color = vec4(tan(posone*2.4f), tanh(posone*2.4f), atan(posone*2.4f), atanh(posone*2.4f));
      break;
    }
    case 84:
    {
      Color = vec4(atan(posone*2.4f, posone*5.7f), sqrt(posone*2.4f), inversesqrt(posone*2.4f), 1.0f);
      break;
    }
    case 85:
    {
      Color = vec4(log(posone*2.4f), log2(posone*2.4f), exp(posone*2.4f), exp2(posone*2.4f));
      break;
    }
    case 86:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      Color = vec4(length(a), length(b), distance(a, b), 1.0f);
      break;
    }
    case 87:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      Color = normalize(a);
      break;
    }
    case 88:
    {
      vec4 a = vec4(posone*2.4f, posone*2.5f, posone*2.6f, posone*2.7f);
      vec4 b = vec4(zerof+2.5f, zerof+2.5f, zerof+2.5f, zerof+2.5f);
      Color = refract(a, b, zerof+3.1f);
      break;
    }
    case 89:
    {
      Color = vec4(fma(zerof+2.4f, posone*0.1f, posone*8.3f),
                   fma(zerof+2.4f, posone*0.0f, posone*8.3f),
                   fma(zerof+3.675f, posone*9.703f, posone*1.45f),
                   ((zerof+3.675f) * (posone*9.703f)) + posone*1.45f);
      break;
    }
    case 90:
    {
      Color = vec4(step(posone*2.6f, zerof+2.4f),
                   step(posone*2.6f, zerof+2.5f),
                   step(posone*2.6f, zerof+2.6f),
                   step(posone*2.6f, zerof+2.7f));
      break;
    }
    case 91:
    {
      Color = vec4(smoothstep(posone*2.0f, posone*2.6f, zerof+1.9f),
                   smoothstep(posone*2.0f, posone*2.6f, zerof+2.0f),
                   smoothstep(posone*2.0f, posone*2.6f, zerof+2.1f),
                   smoothstep(posone*2.0f, posone*2.6f, zerof+2.3f));
      break;
    }
    case 92:
    {
      Color = vec4(smoothstep(posone*2.0f, posone*2.6f, zerof+2.4f),
                   smoothstep(posone*2.0f, posone*2.6f, zerof+2.5f),
                   smoothstep(posone*2.0f, posone*2.6f, zerof+2.6f),
                   smoothstep(posone*2.0f, posone*2.6f, zerof+2.8f));
      break;
    }
    case 93:
    {
      vec4 N = vec4(posone*1.4f, posone*2.8f, posone*5.6f, posone*4.4f);
      vec4 I = vec4(posone*3.7f, posone*2.2f, posone*6.1f, posone*9.5f);
      vec4 Nref = vec4(posone*6.4f, posone*7.5f, posone*8.3f, posone*0.9f);
      Color = faceforward(N, I, Nref);
      break;
    }
    case 94:
    {
      vec4 N = vec4(posone*1.4f, posone*2.8f, posone*5.6f, posone*4.4f);
      vec4 I = vec4(posone*3.7f, posone*2.2f, posone*6.1f, posone*9.5f);
      Color = reflect(N, I);
      break;
    }
    case 95:
    {
      Color = vec4(ldexp(posone*1.4f, zeroi-3),
                   ldexp(posone*2.8f, zeroi+0),
                   ldexp(posone*5.6f, zeroi+3),
                   ldexp(posone*4.4f, zeroi+7));
      break;
    }
    case 96:
    {
      uint a = zerou + 0xb0b0b0b0;
      uint b = zerou + 0x12345678;

      // add and sub with no carry/borrow
      uint y;
      uint x = uaddCarry(a, b, y);
      uint w;
      uint z = usubBorrow(a, b, w);

      Color = vec4(float(x), float(y), float(z), float(w));
      break;
    }
    case 97:
    {
      uint a = zerou + 0xb0b0b0b0;
      uint b = zerou + 0xdeadbeef;

      // add and sub with carry/borrow
      uint y;
      uint x = uaddCarry(a, b, y);
      uint w;
      uint z = usubBorrow(a, b, w);

      Color = vec4(float(x), float(y), float(z), float(w));
      break;
    }
    case 98:
    {
      uint a = zerou + 0xb0b0b0b0;
      uint b = zerou + 0xdeadbeef;

      // add and sub with carry/borrow
      uint y;
      uint x = uaddCarry(a, b, y);
      uint w;
      uint z = usubBorrow(a, b, w);

      Color = vec4(float(x), float(y), float(z), float(w));
      break;
    }
    case 99:
    {
      uint a = zerou + 0x1234;
      uint b = zerou + 0x5678;
      int c = zeroi + 0x1234;
      int d = zeroi + 0x5678;

      // positive mul with no overflow
      uint x, y;
      umulExtended(a, b, y, x);
      int z, w;
      imulExtended(c, d, w, z);

      Color = vec4(float(x), float(y), float(z), float(w));
      break;
    }
    case 100:
    {
      uint a = zerou + 0x123456;
      uint b = zerou + 0x78abcd;
      int c = zeroi + 0x123456;
      int d = zeroi + 0x78abcd;

      // positive mul with overflow
      uint x, y;
      umulExtended(a, b, y, x);
      int z, w;
      imulExtended(c, d, w, z);

      Color = vec4(float(x), float(y), float(z), float(w));
      break;
    }
    case 101:
    {
      int a = zeroi - 0x1234;
      int b = zeroi - 0x5678;
      int c = zeroi - 0x123456;
      int d = zeroi - 0x78abcd;

      // negative mul with and without overflow
      int x, y;
      imulExtended(a, b, y, x);
      int z, w;
      imulExtended(c, d, w, z);

      Color = vec4(float(x), float(y), float(z), float(w));
      break;
    }
)EOSHADER"
                            R"EOSHADER(
    case 102:
    {
      uint a = zerou + 0x0dadbeef;
      int b = zeroi + 0x0dadbeef;

      Color = vec4(float(findLSB(a)), float(findLSB(b)), float(findMSB(a)), float(findMSB(b)));
      break;
    }
    case 103:
    {
      int a = zeroi - 0x0dadbeef;

      Color = vec4(float(findLSB(a)), float(findLSB(zeroi)), float(findMSB(a)), float(findMSB(zeroi)));
      break;
    }
    case 104:
    {
      uint a = zerou + 0x44b82a24;
      int b = zeroi + 0x44b82a24;

      Color = vec4(float(bitCount(a)), float(bitCount(b)), uintBitsToFloat(bitfieldReverse(a)), intBitsToFloat(bitfieldReverse(b)));
      break;
    }
    case 105:
    {
      uint a = zerou + 0x44b82a24;
      int b = zeroi + 0x44b82a24;
      uint af = zerou+0xffffffff;
      int bf = zeroi-1;

      Color = vec4(float(bitfieldExtract(a, 4, 5)), float(bitfieldExtract(b, 4, 5)),
                   uintBitsToFloat(bitfieldInsert(a, af, 4, 5)), intBitsToFloat(bitfieldInsert(b, bf, 4, 5)));
      break;
    }
    case 106:
    {
      Color = vec4(float(textureQueryLevels(queryTest)), float(textureSamples(queryTestMS)), 0.0f, 1.0f);
      break;
    }
    case 107:
    {
      Color = vec4(vec3(textureSize(queryTest, 0)), 1.0f);
      break;
    }
    case 108:
    {
      Color = vec4(vec3(textureSize(queryTest, 1)), 1.0f);
      break;
    }
    case 109:
    {
      Color = vec4(vec3(textureSize(queryTestMS)), 1.0f);
      break;
    }
    case 110:
    {
      Color = vec4(vec3(textureSize(queryTestMS)), 1.0f);
      break;
    }
    case 111:
    {
      Color = texelFetch(texBuffer, int(zeroi+2));
      break;
    }
    case 112:
    {
      float x = texture(sampler2DShadow(depthImage, shadowSampler), vec3(inpos, 0.1f));
      float y = texture(sampler2DShadow(depthImage, shadowSampler), vec3(inpos, 0.3f));
      float z = texture(sampler2DShadow(depthImage, shadowSampler), vec3(inpos, 0.7f));
      float w = texture(sampler2DShadow(depthImage, shadowSampler), vec3(inpos, 0.9f));
      Color = vec4(x, y, z, w);
      break;
    }
    case 113:
    {
      vec2 coord = vec2(zerof + 0.6, zerof + 0.43);

      Color = textureGather(linearSampledImage, coord, 0);
      break;
    }
    case 114:
    {
      vec2 coord = vec2(zerof + 0.6, zerof + 0.43);

      Color = textureGather(linearSampledImage, coord, 1);
      break;
    }
    case 115:
    {
      vec2 coord = vec2(zerof + 0.6, zerof + 0.43);

      Color = textureGather(linearSampledImage, coord, 2);
      break;
    }
    case 116:
    {
      vec2 coord = vec2(zerof + 0.6, zerof + 0.43);

      Color = textureGather(sampler2DShadow(depthImage, shadowSampler), coord, 0.8f);
      break;
    }
    case 117:
    {
      uint packed = packHalf2x16(cbuf.halfPackSource.xy);

      Color = vec4(float((packed & 0xff000000) >> 24),
                   float((packed & 0x00ff0000) >> 16),
                   float((packed & 0x0000ff00) >>  8),
                   float((packed & 0x000000ff) >>  0));
      break;
    }
    case 118:
    {
      vec2 unpacked = unpackHalf2x16(cbuf.halfUnpackSource);

      Color = unpacked.xyxy;
      break;
    }
    case 119:
    {
      uint packed = packUnorm2x16(cbuf.unorm2PackSource.xy);

      Color = vec4(float((packed & 0xff000000) >> 24),
                   float((packed & 0x00ff0000) >> 16),
                   float((packed & 0x0000ff00) >>  8),
                   float((packed & 0x000000ff) >>  0));
      break;
    }
    case 120:
    {
      uint packed = packUnorm4x8(cbuf.unorm4PackSource);

      Color = vec4(float((packed & 0xff000000) >> 24),
                   float((packed & 0x00ff0000) >> 16),
                   float((packed & 0x0000ff00) >>  8),
                   float((packed & 0x000000ff) >>  0));
      break;
    }
    case 121:
    {
      uint packed = packSnorm2x16(cbuf.snorm2PackSource.xy);

      Color = vec4(float((packed & 0xff000000) >> 24),
                   float((packed & 0x00ff0000) >> 16),
                   float((packed & 0x0000ff00) >>  8),
                   float((packed & 0x000000ff) >>  0));
      break;
    }
    case 122:
    {
      uint packed = packSnorm4x8(cbuf.snorm4PackSource);

      Color = vec4(float((packed & 0xff000000) >> 24),
                   float((packed & 0x00ff0000) >> 16),
                   float((packed & 0x0000ff00) >>  8),
                   float((packed & 0x000000ff) >>  0));
      break;
    }
    case 123:
    {
      vec2 unpacked = unpackUnorm2x16(cbuf.unormUnpackSource);

      Color = unpacked.xyxy;
      break;
    }
    case 124:
    {
      vec4 unpacked = unpackUnorm4x8(cbuf.unormUnpackSource);

      Color = unpacked;
      break;
    }
    case 125:
    {
      vec2 unpacked = unpackSnorm2x16(cbuf.snormUnpackSource);

      Color = unpacked.xyxy;
      break;
    }
    case 126:
    {
      vec4 unpacked = unpackSnorm4x8(cbuf.snormUnpackSource);

      Color = unpacked;
      break;
    }
    case 127:
    {
      uint len = storebuf.arr.length();
      Color = vec4(float(len), float(len), float(len), float(len));
      break;
    }
    case 128:
    {
      // test storage buffer write here, we'll read from it in GLSL test 2
      storebuf.x = vec4(3.1f, 4.1f, 5.9f, 2.6f);
      storebuf.y.val = uvec4(31, 41, 59, 26);
      storebuf.arr[flatData.intval - flatData.test] = vec4(inpos, inposIncreased);

      Color = storebuf.x;
      break;
    }
    case 129:
    {
      Color = textureProj(linearSampledImage, vec3(inpos, 0.5f));
      break;
    }
    case 130:
    {
      Color.xy = textureQueryLod(linearSampledImage, inpos);
      Color.zw = textureQueryLod(linearSampledImage, vec2(1.0f, 1.0f)/inpos);
      break;
    }
    case 131:
    {
      Color = vec4(vec2(imageSize(storeImage)), 0.0f, 1.0f);
      break;
    }
    case 132:
    {
      Color = vec4(float(imageSize(storeTexBuffer)), 0.0f, 0.0f, 1.0f);
      break;
    }
    case 133:
    {
      imageStore(storeImage, ivec2(zeroi+1,zeroi+3), vec4(3.1f, 4.1f, 5.9f, 2.6f));
      Color = imageLoad(storeImage, ivec2(zeroi+1,zeroi+3));
      break;
    }
#if TEST_DESC_INDEXING
    case 134:
    {
      ivec2 coord = ivec2(zeroi + 20, zeroi + 20);

      Color = texelFetch(sampledImages[1], coord, 0);
      break;
    }
    case 135:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImages[2], pointSamplers[3]), coord, 0.0);
      break;
    }
    case 136:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImages[2], linearSamplers[3]), coord, 0.0);
      break;
    }
    case 137:
    {
      Color = texture(linearSampledImages[4], inpos.xy);
      break;
    }
    case 138:
    {
      ivec2 coord = ivec2(zeroi + 20, zeroi + 20);

      Color = texelFetch(sampledImages[cbuf.uniformIndex+1], coord, 0);
      break;
    }
    case 139:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImages[cbuf.uniformIndex+2], pointSamplers[cbuf.uniformIndex+3]), coord, 0.0);
      break;
    }
    case 140:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImages[cbuf.uniformIndex+2], linearSamplers[cbuf.uniformIndex+3]), coord, 0.0);
      break;
    }
    case 141:
    {
      Color = texture(linearSampledImages[cbuf.uniformIndex+4], inpos.xy);
      break;
    }
    case 142:
    {
      ivec2 coord = ivec2(zeroi + 20, zeroi + 20);

      Color = texelFetch(sampledImages[nonuniformEXT(zeroi)+9], coord, 0);
      break;
    }
    case 143:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImages[nonuniformEXT(zeroi)+10], pointSamplers[nonuniformEXT(zeroi)+11]), coord, 0.0);
      break;
    }
    case 144:
    {
      vec2 coord = vec2(zerof + 0.5, zerof + 0.145);

      Color = textureLod(sampler2D(sampledImages[nonuniformEXT(zeroi)+10], linearSamplers[nonuniformEXT(zeroi)+11]), coord, 0.0);
      break;
    }
    case 145:
    {
      Color = texture(linearSampledImages[nonuniformEXT(zeroi)+12], inpos.xy);
      break;
    }
    case 146:
    {
      Color = vec4(float(textureQueryLevels(queryTests[0])), float(textureSamples(queryTestsMS[0])), 0.0f, 1.0f);
      break;
    }
    case 147:
    {
      Color = vec4(float(textureQueryLevels(queryTests[zeroi+3])), float(textureSamples(queryTestsMS[zeroi+3])), 0.0f, 1.0f);
      break;
    }
    case 148:
    {
      Color = vec4(float(textureQueryLevels(queryTests[nonuniformEXT(zeroi)+5])), float(textureSamples(queryTestsMS[nonuniformEXT(zeroi)+5])), 0.0f, 1.0f);
      break;
    }
    case 149:
    {
      uint len = storebufs[zeroi+7].arr.length();
      Color = vec4(float(len), float(len), float(len), float(len));
      break;
    }
    case 150:
    {
      // test storage buffer write here, we'll read from it in GLSL test 2
      storebufs[zeroi+7].x = vec4(3.1f, 4.1f, 5.9f, 2.6f);
      storebufs[zeroi+7].y.val = uvec4(31, 41, 59, 26);
      storebufs[zeroi+7].arr[flatData.intval - flatData.test] = vec4(inpos, inposIncreased);

      Color = storebufs[zeroi+7].x;
      break;
    }
    case 151:
    {
      imageStore(storeImages[zeroi+7], ivec2(zeroi+1,zeroi+3), vec4(3.1f, 4.1f, 5.9f, 2.6f));
      Color = imageLoad(storeImages[zeroi+7], ivec2(zeroi+1,zeroi+3));
      break;
    }
    case 152:
    {
      float x = texture(sampler2DShadow(sampledImages[zeroi+5], shadowSamplers[zeroi+8]), vec3(inpos, 0.1f));
      float y = texture(sampler2DShadow(sampledImages[zeroi+5], shadowSamplers[zeroi+8]), vec3(inpos, 0.3f));
      float z = texture(sampler2DShadow(sampledImages[zeroi+5], shadowSamplers[zeroi+8]), vec3(inpos, 0.7f));
      float w = texture(sampler2DShadow(sampledImages[zeroi+5], shadowSamplers[zeroi+8]), vec3(inpos, 0.9f));
      Color = vec4(x, y, z, w);
      break;
    }
#endif
)EOSHADER"
                            R"EOSHADER(
    case 153:
    {
      vec3 cubeCoord = vec3(1.0f, -0.3f, 0.9f);
      Color = textureLod(cubeSampler, cubeCoord, 0.0f);
      break;
    }
    case 154:
    {
      vec3 cubeCoord = vec3(-1.0f, -0.3f, 0.9f);
      Color = textureLod(cubeSampler, cubeCoord, 0.0f);
      break;
    }
    case 155:
    {
      vec3 cubeCoord = vec3(-1.0f, 0.3f, 0.9f);
      Color = textureLod(cubeSampler, cubeCoord, 0.0f);
      break;
    }
    case 156:
    {
      vec3 cubeCoord = vec3(-1.0f, 0.3f, -0.9f);
      Color = textureLod(cubeSampler, cubeCoord, 0.0f);
      break;
    }
    case 157:
    {
      uint old = atomicAdd(atomicbuf.data[flatGlobalCoord].x, flatGlobalCoord);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 158:
    {
      uint old = atomicOr(atomicbuf.data[flatGlobalCoord].x, 0x55555555U);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 159:
    {
      uint old = atomicXor(atomicbuf.data[flatGlobalCoord].x, 0x55555555U);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 160:
    {
      uint old = atomicMax(atomicbuf.data[flatGlobalCoord].x, 0x55555555U);
      uint old2 = atomicMax(atomicbuf.data[flatGlobalCoord].y, 0x38383838U);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff),
                   float(old2 & 0xfffff), float(atomicbuf.data[flatGlobalCoord].y & 0xfffff));
      break;
    }
    case 161:
    {
      uint old = atomicMin(atomicbuf.data[flatGlobalCoord].x, 0x55555555U);
      uint old2 = atomicMin(atomicbuf.data[flatGlobalCoord].y, 0x38383838U);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff),
                   float(old2 & 0xfffff), float(atomicbuf.data[flatGlobalCoord].y & 0xfffff));
      break;
    }
    case 162:
    {
      uint old = atomicExchange(atomicbuf.data[flatGlobalCoord].x, 0x12345678U);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 163:
    {
      uint old = atomicCompSwap(atomicbuf.data[flatGlobalCoord].x, 0x55555555U, 0x12345678U);
      uint old2 = atomicCompSwap(atomicbuf.data[flatGlobalCoord].y, 0x42424242U, 0x12345678U);
      Color = vec4(float(old & 0xfffff), float(atomicbuf.data[flatGlobalCoord].x & 0xfffff),
                   float(old2 & 0xfffff), float(atomicbuf.data[flatGlobalCoord].y & 0xfffff));
      break;
    }
    case 164:
    {
      uint old = imageAtomicAdd(atomicimg, ivec2(gl_FragCoord), flatGlobalCoord);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 165:
    {
      uint old = imageAtomicOr(atomicimg, ivec2(gl_FragCoord), 0x55555555U);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 166:
    {
      uint old = imageAtomicXor(atomicimg, ivec2(gl_FragCoord), 0x55555555U);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 167:
    {
      uint old = imageAtomicMax(atomicimg, ivec2(gl_FragCoord), 0x55555555U);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 168:
    {
      uint old = imageAtomicMin(atomicimg, ivec2(gl_FragCoord), 0x55555555U);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 169:
    {
      uint old = imageAtomicExchange(atomicimg, ivec2(gl_FragCoord), 0x12345678U);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 170:
    {
      uint old = imageAtomicCompSwap(atomicimg, ivec2(gl_FragCoord), 0x55555555U, 0x12345678U);
      Color = vec4(float(old & 0xfffff), float(imageLoad(atomicimg, ivec2(gl_FragCoord)).x & 0xfffff), 0.0f, 0.0f);
      break;
    }
    case 171:
    {
      vec4 ret = vec4(0,0,0,0);
      // test loop continues
      for(int i=0; i < flatLocalCoord + 5; i++)
      {
        ret.x += 0.1f;
        if(i == 2)
        {
          ret.y += 0.2f;
          continue;
        }
        ret.z += 0.1f;
        if(i == 4)
        {
          continue;
        }
        ret.w += 0.1f;
      }
      Color = ret;
      break;
    }
    case 172:
    {
      vec4 ret = vec4(0,0,0,0);
      // test loop breaks
      for(int i=0; i < flatLocalCoord + 5; i++)
      {
        ret.x += 0.1f;
        if(i == 2)
        {
          break;
        }
        ret.y += 0.2f;
      }
      Color = ret;
      break;
    }
    // test fall through
    case 173:
      Color += vec4(0.5, 0.5, 0.5, 0.5);
    case 174:
    {
      Color += vec4(1.0, 1.0, 1.0, 1.0);
      break;
    }
    case 175:
    {
      // this isn't really intended as a true test but more a convenience for manual testing.
      Color = varscope_test(flatLocalCoord, inpos, inposIncreased);
      break;
    }
    case 176:
    {
      ivec2 coord = ivec2(zeroi + 20, zeroi + 20);
      Color = texelFetch(sampledImages[cbuf.uniformIndex+1], coord, 0);
      mat4 mat;
      // force out of bounds matrix lookup to make sure it doesn't crash
      float temp = mat[int(Color.r)+70][int(Color.g)+80];
      if (int(temp/(temp+10000.0)) == 1)
      {
        Color.r = Color.r;
      }
      Color += vec4(1.0, 1.0, 1.0, 1.0);
      break;
    }
    default: break;
  }
}

)EOSHADER";

  std::string vertex2 = R"EOSHADER(
#version 460 core

layout(location = 0) in vec4 pos;
layout(location = 1) in float zero;
layout(location = 2) in float one;
layout(location = 3) in float negone;
layout(location = 4) in vec2 texcoord;

layout(location = 0, component = 0) flat out uint test;
layout(location = 0, component = 1) flat out int zeroi;
layout(location = 0, component = 2) flat out uint intval;

layout(location = 1, component = 1) out vec3 uv;

struct nested
{
  float c; // location 4
  vec2 d; // location 5
};

struct iostruct
{
  float a; // location 2
  float b; // location 3
  nested n;
};

layout(location = 2) out iostruct str;

layout(location = 6) out mat3 matrix;

layout(location = 9) out vec3 arr[3];

void main()
{
  test = gl_InstanceIndex;
 
  gl_Position = vec4(pos.x + pos.z * float(test % 256), pos.y + pos.w * float(test / 256), 0.0, 1.0);

  zeroi = 0;
  intval = test + 7u;

  uv = vec3(texcoord.xy, pos.x);

  vec4 test = vec4(uv.x + 1.0f, uv.y + 2.0f, uv.x + 3.0f, uv.y + 4.0f);

  str.a = test.x;
  str.b = test.y;
  str.n.c = test.z;
  str.n.d = vec2(test.w, 3.141592f);

  test *= 1.5f;
  
  matrix = mat3((test * 2.0f).xyz, (test * 3.0f).xyz, (test * 4.0f).xyz);

  arr[0] = (test * 5.0f).yzw;
  arr[1] = (test * 6.0f).yzw;
  arr[2] = (test * 7.0f).yzw;
}

)EOSHADER";

  std::string pixel_glsl2 = pixel_glsl_header + R"EOSHADER(

layout(location = 0, component = 0) flat in uint test;
layout(location = 0, component = 1) flat in int zeroi;
layout(location = 0, component = 2) flat in uint intval;

layout(location = 1, component = 1) in vec3 uv;

struct nested
{
  float c; // location 4
  vec2 d; // location 5
};

struct iostruct
{
  float a; // location 2
  float b; // location 3
  nested n;
};

layout(location = 2) in iostruct str;

layout(location = 6) in mat3 matrix;

layout(location = 9) in vec3 arr[3];

layout(location = 0) out vec4 Color;

void main()
{
  float zerof = float(zeroi);
  Color = vec4(0,0,0,0);
  switch(test)
  {
    case 0:
    {
      // test loading from the storage buffer (after a nice big barrier)
      Color = storebuf.x;
      break;
    }
    case 1:
    {
      // test loading from the storage buffer (after a nice big barrier)
      Color = vec4(storebuf.y.val);
      break;
    }
    case 2:
    {
      // test loading from the storage buffer (after a nice big barrier)
      Color = storebuf.arr[intval - test];
      break;
    }
    case 3:
    {
      Color = imageLoad(storeImage, ivec2(zeroi+1,zeroi+3));
      break;
    }
    case 4:
    {
      Color = vec4(test, zeroi, intval, 1.0f);
      break;
    }
    case 5:
    {
      Color = vec4(uv.xyz, 1.0f);
      break;
    }
    case 6:
    {
      Color = vec4(str.a, str.b, str.n.c, length(str.n.d));
      break;
    }
    case 7:
    {
      Color = matrix[0].xyzx;
      break;
    }
    case 8:
    {
      Color = matrix[1].xyzx;
      break;
    }
    case 9:
    {
      Color = matrix[2].xyzx;
      break;
    }
    case 10:
    {
      Color = arr[0].xyzx;
      break;
    }
    case 11:
    {
      Color = arr[1].xyzx;
      break;
    }
    case 12:
    {
      Color = arr[2].xyzx;
      break;
    }
    case 13:
    {
      Color = vec4(0,0,0,0);
      uint loopCount = uint(intval - test);
      loopCount -= (uint(gl_FragCoord.x) % 2u);
      loopCount -= (uint(gl_FragCoord.y) % 2u) * 2u;
      vec2 val = uv.xy;
      for(uint i=0; i < loopCount; i++)
      {
        val += vec2(0.01f, 0.01f);
      }
      Color = dFdxFine(val).xyxy;
      break;
    }
    case 14:
    {
      Color = vec4(0,0,0,0);
      uint loopCount = uint(intval - test);
      loopCount += (uint(gl_FragCoord.x) % 2u);
      loopCount += (uint(gl_FragCoord.y) % 2u) * 2u;
      vec2 val = uv.xy;
      for(uint i=0; i < loopCount; i++)
      {
        val += vec2(0.01f, 0.01f);
      }
      Color = dFdxFine(val).xyxy;
      break;
    }
#if TEST_DESC_INDEXING
    case 15:
    {
      // test loading from the storage buffer (after a nice big barrier)
      Color = storebufs[zeroi+7].x;
      break;
    }
    case 16:
    {
      // test loading from the storage buffer (after a nice big barrier)
      Color = vec4(storebufs[zeroi+7].y.val);
      break;
    }
    case 17:
    {
      // test loading from the storage buffer (after a nice big barrier)
      Color = storebufs[zeroi+7].arr[intval - test];
      break;
    }
    case 18:
    {
      Color = imageLoad(storeImages[zeroi+7], ivec2(zeroi+1,zeroi+3));
      break;
    }
#endif
    case 19:
    {
      Color = gl_FrontFacing ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
      break;
    }
    default: break;
  }
}

)EOSHADER";

  std::string capabilities = "OpCapability Shader\n";
  std::string spv_extensions;
  std::string extinstimport =
      R"EOSHADER(
    %glsl450 = OpExtInstImport "GLSL.std.450"
)EOSHADER";
  std::string executionmodes =
      R"EOSHADER(
               OpExecutionMode %main OriginUpperLeft
)EOSHADER";
  std::string spv_debug =
      R"EOSHADER(
   %filename = OpString "file.foo"
)EOSHADER";
  std::string decorations = R"EOSHADER(
               OpDecorate %flatData Flat
               OpDecorate %flatData Location 1
               OpDecorate %linearData Location 3
               OpDecorate %Color Index 0
               OpDecorate %Color Location 0
               OpDecorate %gl_FragCoord BuiltIn FragCoord

               OpDecorate %rtarray_float4 ArrayStride 16
               OpMemberDecorate %dummy 0 Offset 0
               OpMemberDecorate %dummy 1 Offset 16
               OpMemberDecorate %buftype 0 Offset 0
               OpMemberDecorate %buftype 1 Offset 64
               OpMemberDecorate %buftype 2 Offset 128
               OpMemberDecorate %buftype 3 Offset 144
               OpMemberDecorate %buftype 4 Offset 176

               OpMemberDecorate %buftype 0 MatrixStride 16
               OpMemberDecorate %buftype 0 RowMajor

               OpMemberDecorate %buftype 1 MatrixStride 16
               OpMemberDecorate %buftype 1 ColMajor

               OpDecorate %buftype BufferBlock
               OpDecorate %buffer DescriptorSet 0
               OpDecorate %buffer Binding 15
)EOSHADER";
  std::string typesConstants = R"EOSHADER(
       %void = OpTypeVoid
       %bool = OpTypeBool
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
        %int = OpTypeInt 32 1

     %float2 = OpTypeVector %float 2
     %float3 = OpTypeVector %float 3
     %float4 = OpTypeVector %float 4

     %int2 = OpTypeVector %int 2
     %int3 = OpTypeVector %int 3
     %int4 = OpTypeVector %int 4

     %uint2 = OpTypeVector %uint 2
     %uint3 = OpTypeVector %uint 3
     %uint4 = OpTypeVector %uint 4

   %float2x2 = OpTypeMatrix %float2 2
   %float3x3 = OpTypeMatrix %float3 3
   %float2x4 = OpTypeMatrix %float2 4
   %float4x2 = OpTypeMatrix %float4 2
   %float4x4 = OpTypeMatrix %float4 4

     %mainfunc = OpTypeFunction %void
   %doublerfunc = OpTypeFunction %float %float

%rtarray_float4 = OpTypeRuntimeArray %float4

        %v2f = OpTypeStruct %float2 %float2 %float2 %float %float %float
    %flatv2f = OpTypeStruct %uint %uint

      %child = OpTypeStruct %float4 %float3 %float
     %parent = OpTypeStruct %float4 %child %float4x4

     %f32f32 = OpTypeStruct %float %float
     %f32i32 = OpTypeStruct %float %int

      %dummy = OpTypeStruct %uint4 %uint4
    %buftype = OpTypeStruct %float4x4 %float4x4 %float4 %dummy %rtarray_float4

    %ptr_Input_v2f = OpTypePointer Input %v2f
%ptr_Input_flatv2f = OpTypePointer Input %flatv2f
   %ptr_Input_uint = OpTypePointer Input %uint
    %ptr_Input_int = OpTypePointer Input %int
  %ptr_Input_float = OpTypePointer Input %float
 %ptr_Input_float2 = OpTypePointer Input %float2
 %ptr_Input_float4 = OpTypePointer Input %float4
%ptr_Output_float4 = OpTypePointer Output %float4
  %ptr_Private_int = OpTypePointer Private %int
%ptr_Private_float = OpTypePointer Private %float
%ptr_Private_float4 = OpTypePointer Private %float4
%ptr_Private_float4x4 = OpTypePointer Private %float4x4

%ptr_Function_float = OpTypePointer Function %float

%ptr_Uniform_float = OpTypePointer Uniform %float
%ptr_Uniform_float2 = OpTypePointer Uniform %float2
%ptr_Uniform_float3 = OpTypePointer Uniform %float3
%ptr_Uniform_float4 = OpTypePointer Uniform %float4

%ptr_Uniform_uint = OpTypePointer Uniform %uint
%ptr_Uniform_uint2 = OpTypePointer Uniform %uint2
%ptr_Uniform_uint3 = OpTypePointer Uniform %uint3
%ptr_Uniform_uint4 = OpTypePointer Uniform %uint4

%ptr_Uniform_int = OpTypePointer Uniform %int
%ptr_Uniform_int2 = OpTypePointer Uniform %int2
%ptr_Uniform_int3 = OpTypePointer Uniform %int3
%ptr_Uniform_int4 = OpTypePointer Uniform %int4

%ptr_Uniform_float4x4 = OpTypePointer Uniform %float4x4

%ptr_Uniform_dummy = OpTypePointer Uniform %dummy
%ptr_Uniform_buftype = OpTypePointer Uniform %buftype

  %linearData = OpVariable %ptr_Input_v2f Input
    %flatData = OpVariable %ptr_Input_flatv2f Input
%gl_FragCoord = OpVariable %ptr_Input_float4 Input
       %Color = OpVariable %ptr_Output_float4 Output

    %priv_int = OpVariable %ptr_Private_int Private
  %priv_float = OpVariable %ptr_Private_float Private
  %priv_float4 = OpVariable %ptr_Private_float4 Private
  %priv_float4x4 = OpVariable %ptr_Private_float4x4 Private

      %buffer = OpVariable %ptr_Uniform_buftype Uniform

       %flatv2f_test_idx = OpConstant %int 0
     %flatv2f_intval_idx = OpConstant %int 1

        %v2f_zeroVal_idx = OpConstant %int 0
          %v2f_inpos_idx = OpConstant %int 1
 %v2f_inposIncreased_idx = OpConstant %int 2
        %v2f_tinyVal_idx = OpConstant %int 3
         %v2f_oneVal_idx = OpConstant %int 4
      %v2f_negoneVal_idx = OpConstant %int 5

)EOSHADER";
  std::string functions = R"EOSHADER(

       %doubler = OpFunction %float None %doublerfunc
                  OpLine %filename 123 456
                  OpNoLine
                  OpLine %filename 111 222
    %doubler_in = OpFunctionParameter %float
                  OpNoLine
                  OpLine %filename 99 55
                  OpLine %filename 199 155
 %doubler_begin = OpLabel
                  OpLine %filename 299 255
   %doubler_tmp = OpVariable %ptr_Function_float Function
                  OpLine %filename 399 355
   %doubler_ret = OpFMul %float %float_2_0 %doubler_in
                  OpLine %filename 499 455
                  OpStore %doubler_tmp %doubler_ret
                  OpLine %filename 599 555
  %doubler_ret2 = OpLoad %float %doubler_tmp
                  OpLine %filename 699 655
                  OpReturnValue %doubler_ret2
                  OpLine %filename 799 755
                  OpFunctionEnd
)EOSHADER";
  std::vector<std::string> asm_tests;

  void append_tests(const std::initializer_list<std::string> &tests)
  {
    asm_tests.insert(asm_tests.end(), tests.begin(), tests.end());
  }

  void make_asm_tests()
  {
    std::vector<std::string> ret;

    // test binary float maths operations
    for(const std::string &op : {"OpFAdd", "OpFSub", "OpFMul", "OpFDiv", "OpFMod", "OpFRem"})
    {
      bool div = (op == "OpFDiv" || op == "OpFMod" || op == "OpFRem");
      bool mod = (op == "OpFMod" || op == "OpFRem");
      for(const std::string &a : {"15_75", "4_5"})
      {
        for(const std::string &b : {"15_75", "4_5"})
        {
          // don't test A mod A
          if(mod && a == b)
            continue;

          // test A op B and B op A, with neg/pos and dyn/const
          append_tests({
              fmt::format("%_x = {0} %float %float_{1} %float_{2}\n"
                          "%_y = {0} %float %float_neg{1} %float_{2}\n"
                          "%_z = {0} %float %float_{2} %float_{1}\n"
                          "%_w = {0} %float %float_neg{2} %float_{1}\n"
                          "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
                          op, a, b),
              fmt::format("%_x = {0} %float %float_dyn_{1} %float_dyn_{2}\n"
                          "%_y = {0} %float %float_dyn_neg{1} %float_dyn_{2}\n"
                          "%_z = {0} %float %float_dyn_{2} %float_dyn_{1}\n"
                          "%_w = {0} %float %float_dyn_neg{2} %float_dyn_{1}\n"
                          "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
                          op, a, b),
          });

          if(features.shaderFloat64)
          {
            append_tests({
                fmt::format("%_x = {0} %double %double_{1} %double_{2}\n"
                            "%_y = {0} %double %double_neg{1} %double_{2}\n"
                            "%_z = {0} %double %double_{2} %double_{1}\n"
                            "%_w = {0} %double %double_neg{2} %double_{1}\n"
                            "%_out_double4 = OpCompositeConstruct %double4 %_x %_y %_z %_w\n",
                            op, a, b),
                fmt::format("%_x = {0} %double %double_dyn_{1} %double_dyn_{2}\n"
                            "%_y = {0} %double %double_dyn_neg{1} %double_dyn_{2}\n"
                            "%_z = {0} %double %double_dyn_{2} %double_dyn_{1}\n"
                            "%_w = {0} %double %double_dyn_neg{2} %double_dyn_{1}\n"
                            "%_out_double4 = OpCompositeConstruct %double4 %_x %_y %_z %_w\n",
                            op, a, b),
            });
          }

          // also test 0 op A/B

          append_tests({
              fmt::format("%_x = {0} %float %float_0_0 %float_{1}\n"
                          "%_y = {0} %float %float_0_0 %float_{2}\n"
                          "%_z = {0} %float %float_0_0 %float_{3}{1}\n"
                          "%_w = {0} %float %float_0_0 %float_{3}{2}\n"
                          "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
                          op, a, b, mod ? "" : "neg"),
              fmt::format("%_x = {0} %float %float_dyn_0_0 %float_dyn_{1}\n"
                          "%_y = {0} %float %float_dyn_0_0 %float_dyn_{2}\n"
                          "%_z = {0} %float %float_dyn_0_0 %float_dyn_{3}{1}\n"
                          "%_w = {0} %float %float_dyn_0_0 %float_dyn_{3}{2}\n"
                          "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
                          op, a, b, mod ? "" : "neg"),
          });

          // if this isn't a divide, test A/B op 0
          if(!div)
          {
            append_tests({
                fmt::format("%_x = {0} %float %float_{1} %float_0_0\n"
                            "%_y = {0} %float %float_neg{1} %float_0_0\n"
                            "%_z = {0} %float %float_{2} %float_0_0\n"
                            "%_w = {0} %float %float_neg{2} %float_0_0\n"
                            "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
                            op, a, b),
                fmt::format("%_x = {0} %float %float_dyn_{1} %float_dyn_0_0\n"
                            "%_y = {0} %float %float_dyn_neg{1} %float_dyn_0_0\n"
                            "%_z = {0} %float %float_dyn_{2} %float_dyn_0_0\n"
                            "%_w = {0} %float %float_dyn_neg{2} %float_dyn_0_0\n"
                            "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
                            op, a, b),
            });
          }
        }
      }
    }

    // test binary int maths operations
    for(const std::string &op :
        {"OpIAdd", "OpISub", "OpIMul", "OpSDiv", "OpSMod", "OpSRem", "OpUDiv", "OpUMod"})
    {
      bool div =
          (op == "OpSDiv" || op == "OpSMod" || op == "OpSRem" || op == "OpUDiv" || op == "OpUMod");
      bool mod = (op == "OpSMod" || op == "OpSRem" || op == "OpUMod");
      bool sign = op.find('U') == std::string::npos;
      for(uint32_t a : {15, 4})
      {
        for(uint32_t b : {15, 4})
        {
          // don't test A mod A
          if(mod && a == b)
            continue;

          // test A op B for uint and int (positive)
          append_tests({
              fmt::format("%_x = {0} %uint %uint_{1} %uint_{2}\n"
                          "%_y = {0} %uint %uint_dyn_{1} %uint_{2}\n"
                          "%_z = {0} %uint %uint_{2} %uint_{1}\n"
                          "%_w = {0} %uint %uint_dyn_{2} %uint_{1}\n"
                          "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",
                          op, a, b),
              fmt::format("%_x = {0} %uint %uint_0 %uint_{1}\n"
                          "%_y = {0} %uint %uint_0 %uint_dyn_{1}\n"
                          "%_z = {0} %uint %uint_0 %uint_{2}\n"
                          "%_w = {0} %uint %uint_0 %uint_dyn_{2}\n"
                          "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",
                          op, a, b),
          });

          // if this is a signed op, test negative values too
          if(sign && !mod)
          {
            append_tests({
                fmt::format("%_x = {0} %int %int_{1} %int_{2}\n"
                            "%_y = {0} %int %int_dyn_{1} %int_{2}\n"
                            "%_z = {0} %int %int_{2} %int_{1}\n"
                            "%_w = {0} %int %int_dyn_{2} %int_{1}\n"
                            "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
                            op, a, b),
                fmt::format("%_x = {0} %int %int_0 %int_{1}\n"
                            "%_y = {0} %int %int_0 %int_dyn_{1}\n"
                            "%_z = {0} %int %int_0 %int_{2}\n"
                            "%_w = {0} %int %int_0 %int_dyn_{2}\n"
                            "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
                            op, a, b),
                fmt::format("%_x = {0} %int %int_neg{1} %int_{2}\n"
                            "%_y = {0} %int %int_dyn_neg{1} %int_{2}\n"
                            "%_z = {0} %int %int_neg{2} %int_{1}\n"
                            "%_w = {0} %int %int_dyn_neg{2} %int_{1}\n"
                            "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
                            op, a, b),
                fmt::format("%_x = {0} %int %int_0 %int_neg{1}\n"
                            "%_y = {0} %int %int_0 %int_dyn_neg{1}\n"
                            "%_z = {0} %int %int_0 %int_neg{2}\n"
                            "%_w = {0} %int %int_0 %int_dyn_neg{2}\n"
                            "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
                            op, a, b),
            });
          }

          // if it's not a divide op, test A/B op 0
          if(!div)
          {
            append_tests({
                fmt::format("%_x = {0} %uint %uint_{1} %uint_0\n"
                            "%_y = {0} %uint %uint_{2} %uint_0\n"
                            "%_z = {0} %uint %uint_dyn_{1} %uint_dyn_0\n"
                            "%_w = {0} %uint %uint_dyn_{2} %uint_dyn_0\n"
                            "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",
                            op, a, b),
            });

            // and if it's a signed non-divide op, test -A / -B op 0
            if(sign)
            {
              append_tests({
                  fmt::format("%_x = {0} %int %int_neg{1} %int_0\n"
                              "%_y = {0} %int %int_neg{2} %int_0\n"
                              "%_z = {0} %int %int_dyn_neg{1} %int_dyn_0\n"
                              "%_w = {0} %int %int_dyn_neg{2} %int_dyn_0\n"
                              "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
                              op, a, b),
              });
            }
          }
        }
      }
    }

    // test unary operations
    append_tests({
        "%_x = OpFNegate %float %float_10_0\n"
        "%_y = OpFNegate %float %float_neg10_0\n"
        "%_z = OpFNegate %float %float_dyn_10_0\n"
        "%_w = OpFNegate %float %float_dyn_neg10_0\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",

        "%_x = OpFNegate %float %float_0_0\n"
        "%_y = OpFNegate %float %float_neg0_0\n"
        "%_z = OpFNegate %float %float_dyn_0_0\n"
        "%_w = OpFNegate %float %float_dyn_neg0_0\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",

        "%_x = OpSNegate %int %int_10\n"
        "%_y = OpSNegate %int %int_neg10\n"
        "%_z = OpSNegate %int %int_dyn_10\n"
        "%_w = OpSNegate %int %int_dyn_neg10\n"
        "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",

        "%_x = OpSNegate %int %int_0\n"
        "%_y = OpSNegate %int %int_neg0\n"
        "%_z = OpSNegate %int %int_dyn_0\n"
        "%_w = OpSNegate %int %int_dyn_neg0\n"
        "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
    });

    // test bitwise operations
    append_tests({
        "%_x = OpBitwiseOr %uint %uint_0x1234 %uint_0xb9c5\n"
        "%_y = OpBitwiseXor %uint %uint_0x1234 %uint_0xb9c5\n"
        "%_z = OpBitwiseAnd %uint %uint_0x1234 %uint_0xb9c5\n"
        "%_w = OpNot %uint %uint_0x1234 \n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",

        "%_x = OpBitwiseOr %uint %uint_dyn_0x1234 %uint_dyn_0xb9c5\n"
        "%_y = OpBitwiseXor %uint %uint_dyn_0x1234 %uint_dyn_0xb9c5\n"
        "%_z = OpBitwiseAnd %uint %uint_dyn_0x1234 %uint_dyn_0xb9c5\n"
        "%_w = OpNot %uint %uint_dyn_0xb9c5\n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",

        "%_x = OpBitwiseOr %uint %uint_dyn_0x1234 %uint_0\n"
        "%_y = OpBitwiseXor %uint %uint_dyn_0x1234 %uint_0\n"
        "%_z = OpBitwiseAnd %uint %uint_dyn_0x1234 %uint_0\n"
        "%_w = OpNot %uint %uint_0\n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",

        "%_x = OpBitwiseOr %uint %uint_0 %uint_dyn_0xb9c5\n"
        "%_y = OpBitwiseXor %uint %uint_0 %uint_dyn_0xb9c5\n"
        "%_z = OpBitwiseAnd %uint %uint_0 %uint_dyn_0xb9c5\n"
        "%_w = OpNot %uint %uint_dyn_0xb9c5\n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",
    });

    // test shifts
    for(const std::string &op :
        {"OpShiftLeftLogical", "OpShiftRightLogical", "OpShiftRightArithmetic"})
    {
      for(const std::string &dyn : {"", "_dyn"})
      {
        for(const std::string &intType : {"int", "uint"})
        {
          append_tests({
              fmt::format("%_x = {0} %{1} %{1}{2}_0x1234 %uint_0\n"
                          "%_y = {0} %{1} %{1}{2}_0x1234 %uint_1\n"
                          "%_z = {0} %{1} %{1}{2}_0x1234 %uint_2\n"
                          "%_out_{1}3 = OpCompositeConstruct %{1}3 %_x %_y %_z\n",
                          op, intType, dyn),

              fmt::format("%_x = {0} %{1} %{1}_0x1234 %uint{2}_0\n"
                          "%_y = {0} %{1} %{1}_0x1234 %uint{2}_1\n"
                          "%_z = {0} %{1} %{1}_0x1234 %uint{2}_2\n"
                          "%_out_{1}3 = OpCompositeConstruct %{1}3 %_x %_y %_z\n",
                          op, intType, dyn),

              fmt::format("%_x = {0} %{1} %{1}{2}_0x1234 %uint{2}_0\n"
                          "%_y = {0} %{1} %{1}{2}_0x1234 %uint{2}_1\n"
                          "%_z = {0} %{1} %{1}{2}_0x1234 %uint{2}_2\n"
                          "%_out_{1}3 = OpCompositeConstruct %{1}3 %_x %_y %_z\n",
                          op, intType, dyn),
          });
        }
      }
    }

    // test square 2x2 matrix multiplies
    append_tests({
        R"EOTEST(
       %_cola = OpCompositeConstruct %float2 %randf_0 %randf_1
       %_colb = OpCompositeConstruct %float2 %randf_2 %randf_3
        %_mat = OpCompositeConstruct %float2x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float2 %randf_4 %randf_5   

 %_out_float2 = OpMatrixTimesVector %float2 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float2 %randf_0 %randf_1
       %_colb = OpCompositeConstruct %float2 %randf_2 %randf_3
        %_mat = OpCompositeConstruct %float2x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float2 %randf_4 %randf_5   

 %_out_float2 = OpVectorTimesMatrix %float2 %_vec %_mat
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float2 %randf_0 %randf_1
       %_colb = OpCompositeConstruct %float2 %randf_2 %randf_3
       %_mat1 = OpCompositeConstruct %float2x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float2 %randf_4 %randf_5   

       %_mat2 = OpMatrixTimesScalar %float2x2 %_mat1 %randf_6

 %_out_float2 = OpVectorTimesMatrix %float2 %_vec %_mat2
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float2 %randf_0 %randf_1
       %_colb = OpCompositeConstruct %float2 %randf_2 %randf_3
       %_mat1 = OpCompositeConstruct %float2x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float2 %randf_4 %randf_5   

       %_colc = OpCompositeConstruct %float2 %randf_6 %randf_7
       %_cold = OpCompositeConstruct %float2 %randf_8 %randf_9
       %_mat2 = OpCompositeConstruct %float2x2 %_colc %_cold

       %_mat3 = OpMatrixTimesMatrix %float2x2 %_mat1 %_mat2

 %_out_float2 = OpVectorTimesMatrix %float2 %_vec %_mat3
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float2 %randf_0 %randf_1
       %_colb = OpCompositeConstruct %float2 %randf_2 %randf_3
       %_mat1 = OpCompositeConstruct %float2x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float2 %randf_4 %randf_5   

       %_colc = OpCompositeConstruct %float2 %randf_6 %randf_7
       %_cold = OpCompositeConstruct %float2 %randf_8 %randf_9
       %_mat2 = OpCompositeConstruct %float2x2 %_colc %_cold

       %_mat3 = OpMatrixTimesMatrix %float2x2 %_mat2 %_mat1

 %_out_float2 = OpVectorTimesMatrix %float2 %_vec %_mat3
)EOTEST",
    });

    // test rectangular 2x4 / 4x2 matrix multiplies
    append_tests({
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
        %_mat = OpCompositeConstruct %float4x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

 %_out_float2 = OpVectorTimesMatrix %float2 %_vec %_mat
)EOTEST",
        R"EOTEST(
       %_colc = OpCompositeConstruct %float2 %randf_8 %randf_9
       %_cold = OpCompositeConstruct %float2 %randf_10 %randf_11
       %_cole = OpCompositeConstruct %float2 %randf_12 %randf_13
       %_colf = OpCompositeConstruct %float2 %randf_14 %randf_15
        %_mat = OpCompositeConstruct %float2x4 %_colc %_cold %_cole %_colf

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

 %_out_float2 = OpMatrixTimesVector %float2 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
        %_mat = OpCompositeConstruct %float4x2 %_cola %_colb

        %_vec = OpCompositeConstruct %float2 %randf_16 %randf_17

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_colc = OpCompositeConstruct %float2 %randf_8 %randf_9
       %_cold = OpCompositeConstruct %float2 %randf_10 %randf_11
       %_cole = OpCompositeConstruct %float2 %randf_12 %randf_13
       %_colf = OpCompositeConstruct %float2 %randf_14 %randf_15
        %_mat = OpCompositeConstruct %float2x4 %_colc %_cold %_cole %_colf

        %_vec = OpCompositeConstruct %float2 %randf_16 %randf_17

 %_out_float4 = OpVectorTimesMatrix %float4 %_vec %_mat
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
       %_mat1 = OpCompositeConstruct %float4x2 %_cola %_colb

       %_colc = OpCompositeConstruct %float2 %randf_8 %randf_9
       %_cold = OpCompositeConstruct %float2 %randf_10 %randf_11
       %_cole = OpCompositeConstruct %float2 %randf_12 %randf_13
       %_colf = OpCompositeConstruct %float2 %randf_14 %randf_15
       %_mat2 = OpCompositeConstruct %float2x4 %_colc %_cold %_cole %_colf

        %_mat = OpMatrixTimesMatrix %float4x4 %_mat1 %_mat2

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
       %_mat1 = OpCompositeConstruct %float4x2 %_cola %_colb

       %_colc = OpCompositeConstruct %float4 %randf_8 %randf_9 %randf_10 %randf_11
       %_cold = OpCompositeConstruct %float4 %randf_12 %randf_13 %randf_14 %randf_15
       %_mat2 = OpCompositeConstruct %float4x2 %_colc %_cold

      %_mat2t = OpTranspose %float2x4 %_mat2

        %_mat = OpMatrixTimesMatrix %float4x4 %_mat1 %_mat2t

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7

        %_mat = OpOuterProduct %float4x4 %_cola %_colb

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
        R"EOTEST(
        %_vec = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
 %_out_float4 = OpVectorTimesScalar %float4 %_vec %randf_4
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float2 %randf_0 %randf_1
       %_colb = OpCompositeConstruct %float2 %randf_4 %randf_5
       %_colc = OpCompositeConstruct %float2 %randf_8 %randf_9
       %_cold = OpCompositeConstruct %float2 %randf_12 %randf_13
       %_mat1 = OpCompositeConstruct %float2x2 %_cola %_colb

  %_out_float = OpExtInst %float %glsl450 Determinant %_mat1
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float3 %randf_0 %randf_1 %randf_2
       %_colb = OpCompositeConstruct %float3 %randf_4 %randf_5 %randf_6
       %_colc = OpCompositeConstruct %float3 %randf_8 %randf_9 %randf_10
       %_mat1 = OpCompositeConstruct %float3x3 %_cola %_colb %_colc

  %_out_float = OpExtInst %float %glsl450 Determinant %_mat1
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
       %_colc = OpCompositeConstruct %float4 %randf_8 %randf_9 %randf_10 %randf_11
       %_cold = OpCompositeConstruct %float4 %randf_12 %randf_13 %randf_14 %randf_15
       %_mat1 = OpCompositeConstruct %float4x4 %_cola %_colb %_colc %_cold

  %_out_float = OpExtInst %float %glsl450 Determinant %_mat1
)EOTEST",
    });

    // test matrix inverse, but round the result to avoid needing to lower our global precision
    // epsilon
    for(int dim = 2; dim <= 4; dim++)
    {
      std::string test = fmt::format(R"EOTEST(
       %_cola = OpCompositeConstruct %float{0} %randf_0 %randf_1 {1} %randf_2 {2} %randf_3
       %_colb = OpCompositeConstruct %float{0} %randf_4 %randf_5 {1} %randf_6 {2} %randf_7
       %_colc = OpCompositeConstruct %float{0} %randf_8 %randf_9 {1} %randf_10 {2} %randf_11
       %_cold = OpCompositeConstruct %float{0} %randf_12 %randf_13 {1} %randf_14 {2} %randf_15

        %_mat = OpCompositeConstruct %float{0}x{0} %_cola %_colb {1} %_colc {2} %_cold

        %_vec = OpCompositeConstruct %float{0} %randf_16 %randf_17 {1} %randf_18 {2} %randf_19

       %_mat0 = OpExtInst %float{0}x{0} %glsl450 MatrixInverse %_mat
)EOTEST",
                                     dim, dim < 3 ? ";" : "", dim < 4 ? ";" : "");

      int i = 0;
      for(int col = 0; col < dim; col++)
      {
        for(int row = 0; row < dim; row++)
        {
          test += fmt::format(R"EOTEST(
     %_mat{0}{1}a = OpCompositeExtract %float %_mat{2} {0} {1}
     %_mat{0}{1}b = OpFMul %float %_mat{0}{1}a %float_500_0
     %_mat{0}{1}c = OpExtInst %float %glsl450 RoundEven %_mat{0}{1}b
     %_mat{0}{1}d = OpFDiv %float %_mat{0}{1}c %float_500_0

         %_mat{3} = OpCompositeInsert %float{4}x{4} %_mat{0}{1}d %_mat{2} {0} {1}
)EOTEST",
                              col, row, i, i + 1, dim);
          i++;
        }
      }

      test += fmt::format("%_out_float{0} = OpMatrixTimesVector %float{0} %_mat{1} %_vec\n", dim, i);

      asm_tests.push_back(test);
    }

    // test OpVectorShuffle
    append_tests({
        "%_out_float4 = OpVectorShuffle %float4 %float4_0000 %float4_1234 7 6 0 1",
        "%_out_float4 = OpVectorShuffle %float4 %float4_0000 %float4_dyn_1234 7 6 0 1",
        "%_out_float4 = OpVectorShuffle %float4 %float4_dyn_0000 %float4_1234 7 6 0 1",
        "%_out_float4 = OpVectorShuffle %float4 %float4_dyn_0000 %float4_dyn_1234 7 6 0 1",
        "%_out_float3 = OpVectorShuffle %float3 %float3_000 %float3_123 3 4 5",
        "%_out_float2 = OpVectorShuffle %float2 %float2_00 %float2_12 2 3",

        // test 0xffffffff component inputs
        "%_tmp = OpVectorShuffle %float4 %float4_0000 %float4_1234 5 4 4294967295 4294967295\n"
        "%_out_float4 = OpVectorShuffle %float4 %_tmp %float4_dyn_1234 0 1 4 5",
    });

    // test OpVectorExtractDynamic
    append_tests({
        "%_x = OpVectorExtractDynamic %float %float4_dyn_1234 %uint_dyn_1\n"
        "%_y = OpVectorExtractDynamic %float %float4_dyn_1234 %uint_dyn_3\n"
        "%_z = OpVectorExtractDynamic %float %float4_dyn_1234 %uint_dyn_2\n"
        "%_w = OpVectorExtractDynamic %float %float4_dyn_0000 %uint_dyn_2\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
    });

    // test OpVectorInsertDynamic
    append_tests({
        "%_out_float4 = OpVectorInsertDynamic %float4 %float4_dyn_1234 %float_dyn_8_8 %uint_dyn_1",
        "%_out_float4 = OpVectorInsertDynamic %float4 %float4_dyn_1234 %float_dyn_8_8 %uint_dyn_2",
        "%_out_float4 = OpVectorInsertDynamic %float4 %float4_dyn_1234 %float_dyn_8_8 %uint_dyn_0",
    });

    // test OpCompositeInsert on vectors
    append_tests({
        "          %_b = OpCompositeInsert %float4 %float_15_0 %float4_0000 2\n"
        "          %_c = OpCompositeInsert %float4 %float_8_8 %_b 1\n"
        "          %_d = OpCompositeInsert %float4 %float_6_1 %_c 3\n"
        "%_out_float4 = OpCompositeInsert %float4 %float_2_222 %_d 0\n",

        "          %_b = OpCompositeInsert %float4 %float_dyn_15_0 %float4_dyn_0000 2\n"
        "          %_c = OpCompositeInsert %float4 %float_dyn_8_8 %_b 1\n"
        "          %_d = OpCompositeInsert %float4 %float_dyn_6_1 %_c 3\n"
        "%_out_float4 = OpCompositeInsert %float4 %float_dyn_2_222 %_d 0\n",
    });

    // test OpCompositeExtract on vectors
    append_tests({
        "%_out_float = OpCompositeExtract %float %float4_dyn_1234 0",
        "%_out_float = OpCompositeExtract %float %float4_dyn_1234 1",
        "%_out_float = OpCompositeExtract %float %float4_dyn_1234 3",
    });

    // test OpCompositeInsert on structs
    asm_tests.push_back(R"EOTEST(
   %_a = OpCompositeConstruct %float4 %float_dyn_4_2 %float_dyn_1_0 %float_dyn_9_5 %float_dyn_0_01
   %_b = OpCompositeConstruct %float3 %float_dyn_3_5 %float_dyn_5_3 %float_dyn_6_2

   %_c = OpVectorShuffle %float4 %_a %_a 3 2 0 1
   %_d = OpVectorShuffle %float4 %_a %_a 0 1 3 2
   %_e = OpVectorShuffle %float4 %_a %_a 2 0 1 3
   %_f = OpVectorShuffle %float4 %_a %_a 3 1 2 0
   %_g = OpVectorShuffle %float4 %_a %_a 1 3 0 2

%_parent1 = OpCompositeInsert %parent %_a %null_parent 0

%_parent2 = OpCompositeInsert %parent %_a %_parent1 1 0
%_parent3 = OpCompositeInsert %parent %_b %_parent2 1 1
%_parent4 = OpCompositeInsert %parent %float_dyn_9_9 %_parent3 1 2

%_parent5 = OpCompositeInsert %parent %_c %_parent4 2 0
%_parent6 = OpCompositeInsert %parent %_d %_parent5 2 1
%_parent7 = OpCompositeInsert %parent %_e %_parent6 2 2
%_parent8 = OpCompositeInsert %parent %_g %_parent7 2 3

      %_x = OpCompositeExtract %float %_parent8 0 2
      %_y = OpCompositeExtract %float %_parent8 2 1 3
      %_z = OpCompositeExtract %float %_parent8 1 1 1
      %_w = OpCompositeExtract %float %_parent8 1 0 2

%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w

)EOTEST");

    // test OpBitCast
    append_tests({
        "%_a = OpBitcast %uint %float_dyn_15_0\n"
        "%_neg = OpBitwiseOr %uint %_a %uint_dyn_0x80000000\n"
        "%_out_float = OpBitcast %float %_neg\n",

        "%_result = OpBitwiseOr %uint %uint_dyn_0x4200004d %uint_dyn_0xa28b00\n"
        "%_out_float = OpBitcast %float %_result\n",
    });

    // test ExtInst NMin/NMax/NClamp
    append_tests({
        "%_x = OpExtInst %float %glsl450 NMin %nan %oneVal\n"
        "%_y = OpExtInst %float %glsl450 NMin %oneVal %nan\n"
        "%_z = OpExtInst %float %glsl450 NMin %nan %nan\n"
        "%_w = OpExtInst %float %glsl450 NMin %nan %neginf\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",

        "%_x = OpExtInst %float %glsl450 NMax %nan %oneVal\n"
        "%_y = OpExtInst %float %glsl450 NMax %oneVal %nan\n"
        "%_z = OpExtInst %float %glsl450 NMax %nan %nan\n"
        "%_w = OpExtInst %float %glsl450 NMax %nan %neginf\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",

        "%_out_float = OpExtInst %float %glsl450 NClamp %nan %zerof %oneVal",
    });

    // test ExtInst Modf/ModfStruct and Frexp/FrexpStruct
    append_tests({
        "%_x = OpExtInst %float %glsl450 Modf %float_dyn_123_456 %priv_float\n"
        "%_y = OpLoad %float %priv_float\n"
        "%_tmp = OpExtInst %f32f32 %glsl450 ModfStruct %float_dyn_789_012\n"
        "%_z = OpCompositeExtract %float %_tmp 0\n"
        "%_w = OpCompositeExtract %float %_tmp 1\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",

        "%_x = OpExtInst %float %glsl450 Frexp %float_dyn_123_456 %priv_int\n"
        "%_yi = OpLoad %int %priv_int\n"
        "%_y = OpConvertSToF %float %_yi\n"
        "%_tmp = OpExtInst %f32i32 %glsl450 FrexpStruct %float_dyn_789_012\n"
        "%_z = OpCompositeExtract %float %_tmp 0\n"
        "%_wi = OpCompositeExtract %int %_tmp 1\n"
        "%_w = OpConvertSToF %float %_wi\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",
    });

    // test float <-> int conversions
    append_tests({
        "%_x = OpConvertUToF %float %uint_dyn_1234\n"
        "%_y = OpConvertSToF %float %int_dyn_1234\n"
        "%_z = OpConvertSToF %float %int_dyn_neg1234\n"
        "%_w = OpConvertUToF %float %uint_dyn_0\n"
        "%_out_float4 = OpCompositeConstruct %float4 %_x %_y %_z %_w\n",

        "%_x = OpConvertFToU %uint %float_dyn_1_0\n"
        "%_y = OpConvertFToU %uint %float_dyn_0_0\n"
        "%_z = OpConvertFToU %uint %float_dyn_1_1\n"
        "%_w = OpConvertFToU %uint %float_dyn_1_3\n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",

        "%_x = OpConvertFToU %uint %float_dyn_1_0\n"
        "%_y = OpConvertFToU %uint %float_dyn_1_5\n"
        "%_z = OpConvertFToU %uint %float_dyn_0_5\n"
        "%_w = OpConvertFToU %uint %float_dyn_1_7\n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",

        "%_x = OpConvertFToS %int %float_dyn_1_0\n"
        "%_y = OpConvertFToS %int %float_dyn_0_0\n"
        "%_z = OpConvertFToS %int %float_dyn_neg1_0\n"
        "%_w = OpConvertFToS %int %float_dyn_1_3\n"
        "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",

        "%_x = OpConvertFToS %int %float_dyn_1_0\n"
        "%_y = OpConvertFToS %int %float_dyn_1_5\n"
        "%_z = OpConvertFToS %int %float_dyn_0_5\n"
        "%_w = OpConvertFToS %int %float_dyn_neg1_5\n"
        "%_out_int4 = OpCompositeConstruct %int4 %_x %_y %_z %_w\n",
    });

    // test copies
    append_tests({
        "OpCopyMemory %Color %gl_FragCoord\n"
        "; no_out\n",

        "%_src = OpAccessChain %ptr_Uniform_float4 %buffer %uint_2\n"
        "%_dst = OpAccessChain %ptr_Uniform_float4 %buffer %uint_4 %uint_3\n"
        "OpCopyMemory %_dst %_src\n"
        "OpCopyMemory %Color %_src\n"
        "; no_out\n",

        "%frag = OpLoad %float4 %gl_FragCoord\n"
        "%_out_float4 = OpCopyObject %float4 %frag\n",
    });

    // test SSBO pointers
    append_tests({
        "%_y = OpAccessChain %ptr_Uniform_dummy %buffer %uint_3\n"
        "%_src = OpAccessChain %ptr_Uniform_uint4 %_y %uint_0\n"
        "%_dst = OpAccessChain %ptr_Uniform_uint4 %_y %uint_1\n"
        "%_tmp = OpLoad %uint4 %_src\n"
        "OpStore %_dst %_tmp\n"
        "%_out_uint4 = OpLoad %uint4 %_dst\n",
    });

    // disabled while shaderc has a bug that doesn't respect the target environment
    /*
    if(vk_version >= 0x12)
    {
      append_tests({
          "%frag = OpLoad %float4 %gl_FragCoord\n"
          "%_out_float4 = OpCopyLogical %float4 %frag\n",
      });
    }
    */

    if(features.shaderFloat64)
    {
      // test pack/unpack from double
      append_tests({
          "%_ptr = OpAccessChain %ptr_Uniform_uint2 %cbuffer %uint_16\n"
          "%_double_pack_source = OpLoad %uint2 %_ptr\n"
          "%_out_double = OpExtInst %double %glsl450 PackDouble2x32 %_double_pack_source\n",

          "%_ptr = OpAccessChain %ptr_Uniform_double %cbuffer %uint_17\n"
          "%_double_unpack_source = OpLoad %double %_ptr\n"
          "%_out_uint2 = OpExtInst %uint2 %glsl450 UnpackDouble2x32 %_double_unpack_source\n",

          "%_ptr = OpAccessChain %ptr_Uniform_double %cbuffer %uint_17\n"
          "%_pi = OpLoad %double %_ptr\n"
          "%_two = OpFConvert %double %float_2_0\n"
          "%_out_double = OpFMul %double %_pi %_two\n",
      });
    }

    // test pointers into columns of matrices

    append_tests({
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
       %_colc = OpCompositeConstruct %float4 %randf_8 %randf_9 %randf_10 %randf_11
       %_cold = OpCompositeConstruct %float4 %randf_12 %randf_13 %randf_14 %randf_15

       %_ptra = OpAccessChain %ptr_Private_float4 %priv_float4x4 %uint_0
       %_ptrb = OpAccessChain %ptr_Private_float4 %priv_float4x4 %uint_1
       %_ptrc = OpAccessChain %ptr_Private_float4 %priv_float4x4 %uint_2
       %_ptrd = OpAccessChain %ptr_Private_float4 %priv_float4x4 %uint_3

                OpStore %_ptra %_cola
                OpStore %_ptrb %_colb
                OpStore %_ptrc %_colc
                OpStore %_ptrd %_cold

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

        %_mat = OpLoad %float4x4 %priv_float4x4

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
       %_colc = OpCompositeConstruct %float4 %randf_8 %randf_9 %randf_10 %randf_11
       %_cold = OpCompositeConstruct %float4 %randf_12 %randf_13 %randf_14 %randf_15

       %_ptra = OpAccessChain %ptr_Uniform_float4 %buffer %uint_0 %uint_0
       %_ptrb = OpAccessChain %ptr_Uniform_float4 %buffer %uint_0 %uint_1
       %_ptrc = OpAccessChain %ptr_Uniform_float4 %buffer %uint_0 %uint_2
       %_ptrd = OpAccessChain %ptr_Uniform_float4 %buffer %uint_0 %uint_3

                OpStore %_ptra %_cola
                OpStore %_ptrb %_colb
                OpStore %_ptrc %_colc
                OpStore %_ptrd %_cold

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

     %_ptrmat = OpAccessChain %ptr_Uniform_float4x4 %buffer %uint_0
        %_mat = OpLoad %float4x4 %_ptrmat

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
        R"EOTEST(
       %_cola = OpCompositeConstruct %float4 %randf_0 %randf_1 %randf_2 %randf_3
       %_colb = OpCompositeConstruct %float4 %randf_4 %randf_5 %randf_6 %randf_7
       %_colc = OpCompositeConstruct %float4 %randf_8 %randf_9 %randf_10 %randf_11
       %_cold = OpCompositeConstruct %float4 %randf_12 %randf_13 %randf_14 %randf_15

       %_ptra = OpAccessChain %ptr_Uniform_float4 %buffer %uint_1 %uint_0
       %_ptrb = OpAccessChain %ptr_Uniform_float4 %buffer %uint_1 %uint_1
       %_ptrc = OpAccessChain %ptr_Uniform_float4 %buffer %uint_1 %uint_2
       %_ptrd = OpAccessChain %ptr_Uniform_float4 %buffer %uint_1 %uint_3

                OpStore %_ptra %_cola
                OpStore %_ptrb %_colb
                OpStore %_ptrc %_colc
                OpStore %_ptrd %_cold

        %_vec = OpCompositeConstruct %float4 %randf_16 %randf_17 %randf_18 %randf_19

     %_ptrmat = OpAccessChain %ptr_Uniform_float4x4 %buffer %uint_1
        %_mat = OpLoad %float4x4 %_ptrmat

 %_out_float4 = OpMatrixTimesVector %float4 %_mat %_vec
)EOTEST",
    });

    // test variables with initialisers
    append_tests({
        R"EOTEST(
                 ; this has a constant initialiser, so should already be ready
 %_out_float4 = OpLoad %float4 %priv_float4_init
)EOTEST",
        R"EOTEST(
                 ; this is uninitialised, but unforuntately that means we can't test our debugging
                 ; against the real thing when it's undefined. But we can at least expose it so that
                 ; when manually checking we see the uninitialised values
     %_uninit = OpLoad %float4 %priv_float4
          %_x = OpExtInst %float4 %glsl450 NClamp %_uninit %float4_0000 %float4_0000
 %_out_float4 = OpFAdd %float4 %_x %float4_1234
)EOTEST",
        R"EOTEST(
                 ; this is uninitialised, but unforuntately that means we can't test our debugging
                 ; against the real thing when it's undefined. But we can at least expose it so that
                 ; when manually checking we see the uninitialised values
     %_uninit = OpLoad %float4 %Color
          %_x = OpExtInst %float4 %glsl450 NClamp %_uninit %float4_0000 %float4_0000
 %_out_float4 = OpFAdd %float4 %_x %float4_1234
)EOTEST",
    });

    // test naming structs. Since we can't easily name auto-generated IDs we use a guid to give the
    // ID a unique name
    append_tests({
        R"EOTEST(
          %_a = OpCompositeConstruct %float4 %float_dyn_4_2 %float_dyn_1_0 %float_dyn_9_5 %float_dyn_0_01

%C14FA880_4F83_4982_BEAD_CE9103446C76 = OpCompositeInsert %parent %_a %null_parent 0

%_out_float4 = OpCompositeExtract %float4 %C14FA880_4F83_4982_BEAD_CE9103446C76 0
)EOTEST",
    });

    spv_debug +=
        "OpName %C14FA880_4F83_4982_BEAD_CE9103446C76 \"C14FA880_4F83_4982_BEAD_CE9103446C76\"\n";

    // test OpPhi
    append_tests({

        // basic simple test
        R"EOTEST(
OpBranch %_toplabel
%_toplabel = OpLabel

%_val = OpDot %float %inpos %float2_12
%_cond = OpFOrdGreaterThan %bool %_val %float_37_0

%_parent1 = OpFMul %float %float_2_0 %float_0_5

OpSelectionMerge %_merge None
OpBranchConditional %_cond %_merge %_branchlabel

%_branchlabel = OpLabel

%_parent2 = OpFMul %float %float_2_0 %float_0_25

OpBranch %_merge

%_merge = OpLabel

; choose either parent1 or parent2, depending on if we branched
%_out_float = OpPhi %float %_parent1 %_toplabel %_parent2 %_branchlabel

OpBranch %_bottomlabel
%_bottomlabel = OpLabel

)EOTEST",

        // test with a function call in each branch to ensure we still track the last block
        // accurately
        R"EOTEST(
OpBranch %_toplabel
%_toplabel = OpLabel

%_val = OpDot %float %inpos %float2_12
%_cond = OpFOrdGreaterThan %bool %_val %float_37_0

%_parent1 = OpFunctionCall %float %doubler %float_0_5

OpSelectionMerge %_merge None
OpBranchConditional %_cond %_merge %_branchlabel

%_branchlabel = OpLabel

%_parent2 = OpFunctionCall %float %doubler %float_0_25

OpBranch %_merge

%_merge = OpLabel

; choose either parent1 or parent2, depending on if we branched
%_out_float = OpPhi %float %_parent1 %_toplabel %_parent2 %_branchlabel

OpBranch %_bottomlabel
%_bottomlabel = OpLabel

)EOTEST",
    });

    // test switch for different integer types
    std::vector<std::string> intTypes = {"int", "uint"};
    std::vector<std::string> caseLiterals = {"0x12345678", "0xF2345678"};
    if(features.shaderInt64)
    {
      intTypes.push_back("i64");
      intTypes.push_back("u64");
      caseLiterals.push_back("0x1234567812345678");
      caseLiterals.push_back("0xF234567812345678");
    }
    for(size_t i = 0; i < intTypes.size(); ++i)
    {
      append_tests({fmt::format(
          "%_test_switch_{0} = OpIAdd %{0} %{0}_0 %{0}_{1}\n"
          "OpSelectionMerge %_break_{0} None\n"
          "OpSwitch %_test_switch_{0} %_default_{0} 2 %_case_{0}_2 {1} %_case_{0}_{1}\n"
          "%_case_{0}_2 = OpLabel\n"
          "OpUnreachable\n"
          "%_case_{0}_{1} = OpLabel\n"
          "%_out_{0} = OpIAdd %{0} %{0}_0 %{0}_7\n"
          "OpBranch %_break_{0}\n"
          "%_default_{0} = OpLabel\n"
          "OpUnreachable\n"
          "%_break_{0} = OpLabel\n",
          intTypes[i], caseLiterals[i])});
    }
  }

  std::string make_pixel_asm()
  {
    std::string switch_str = R"EOSHADER(
               OpSelectionMerge %break None
               OpSwitch %test
                        %default
)EOSHADER";

    std::set<std::string> null_constants;
    std::set<float> float_constants = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    std::set<int32_t> int_constants = {7};
    std::set<uint32_t> uint_constants;
    std::set<int64_t> i64_constants;
    std::set<uint64_t> u64_constants;

    std::string cases;

    for(size_t i = 0; i < asm_tests.size(); i++)
    {
      std::string &test = asm_tests[i];
      // append a newline just so that searching for whitespace always finds it even if the last
      // thing in the test is a %_foo
      test += "\n";

      // add the test's case
      switch_str += fmt::format("{0} %test_{0}\n", i);
      cases += fmt::format("%test_{} = OpLabel\n", i);

      std::string test_suffix = fmt::format("_{}", i);

      // find any identifiers with the prefix %_ in the test, and append _testindex
      size_t offs = test.find("%_");
      while(offs != std::string::npos)
      {
        offs = test.find_first_of("\n\t ", offs);
        test.insert(offs, test_suffix);

        offs = test.find("%_", offs);
      }

      // find any null constants referenced
      offs = test.find("%null_");
      while(offs != std::string::npos)
      {
        offs += 6;    // past %null_
        size_t begin = offs;
        offs = test.find_first_of("\n\t ", offs);
        null_constants.insert(test.substr(begin, offs - begin));

        offs = test.find("%null_", offs);
      }

      // find any float constants referenced
      for(std::string prefix : {"%float_", "%double_", "%half_"})
      {
        offs = test.find(prefix);
        while(offs != std::string::npos)
        {
          offs += prefix.size();

          // we generate dynamic and negative versions of all constants, skip to the first digit
          offs = test.find_first_of("0123456789", offs);

          size_t begin = offs;
          offs = test.find_first_of("\n\t ", offs);

          std::string val = test.substr(begin, offs - begin);

          // convert any _ to a .
          for(char &c : val)
            if(c == '_')
              c = '.';

          float_constants.insert(std::strtof(val.c_str(), NULL));

          offs = test.find(prefix, offs);
        }
      }

      // find any int constants referenced
      offs = test.find("%int_");
      while(offs != std::string::npos)
      {
        offs += 5;    // past %int_

        // we generate dynamic and negative versions of all constants, skip to the first digit
        offs = test.find_first_of("0123456789", offs);

        // handle hex prefix
        int base = 10;
        if(test[offs] == '0' && test[offs + 1] == 'x')
        {
          base = 16;
          offs += 2;
        }

        int32_t val = std::strtol(&test[offs], NULL, base);
        int_constants.insert(val);

        // if it's a hex constant we'll name it in decimal, rename
        if(base == 16)
        {
          size_t end = test.find_first_of("\n\t ", offs);
          test.replace(offs - 2, end - offs + 2, fmt::format("{}", val));
        }

        offs = test.find("%int_", offs);
      }

      // find any uint constants referenced
      offs = test.find("%uint_");
      while(offs != std::string::npos)
      {
        offs += 6;    // past %uint_

        // we generate dynamic and negative versions of all constants, skip to the first digit
        offs = test.find_first_of("0123456789", offs);

        // handle hex prefix
        int base = 10;
        if(test[offs] == '0' && test[offs + 1] == 'x')
        {
          base = 16;
          offs += 2;
        }

        uint32_t val = std::strtoul(&test[offs], NULL, base);
        uint_constants.insert(val);

        // if it's a hex constant we'll name it in decimal, rename
        if(base == 16)
        {
          size_t end = test.find_first_of("\n\t ", offs);
          test.replace(offs - 2, end - offs + 2, fmt::format("{}", val));
        }

        offs = test.find("%uint_", offs);
      }

      // find any i64 constants referenced
      offs = test.find("%i64_");
      while(offs != std::string::npos)
      {
        offs += 5;    // past %i64_

        // we generate dynamic and negative versions of all constants, skip to the first digit
        offs = test.find_first_of("0123456789", offs);

        // handle hex prefix
        int base = 10;
        if(test[offs] == '0' && test[offs + 1] == 'x')
        {
          base = 16;
          offs += 2;
        }

        int64_t val = std::strtoll(&test[offs], NULL, base);
        i64_constants.insert(val);

        // if it's a hex constant we'll name it in decimal, rename
        if(base == 16)
        {
          size_t end = test.find_first_of("\n\t ", offs);
          test.replace(offs - 2, end - offs + 2, fmt::format("{}", val));
        }

        offs = test.find("%i64_", offs);
      }

      // find any u64 constants referenced
      offs = test.find("%u64_");
      while(offs != std::string::npos)
      {
        offs += 5;    // past %u64_

        // we generate dynamic and negative versions of all constants, skip to the first digit
        offs = test.find_first_of("0123456789", offs);

        // handle hex prefix
        int base = 10;
        if(test[offs] == '0' && test[offs + 1] == 'x')
        {
          base = 16;
          offs += 2;
        }

        uint64_t val = std::strtoull(&test[offs], NULL, base);
        u64_constants.insert(val);

        // if it's a hex constant we'll name it in decimal, rename
        if(base == 16)
        {
          size_t end = test.find_first_of("\n\t ", offs);
          test.replace(offs - 2, end - offs + 2, fmt::format("{}", val));
        }

        offs = test.find("%u64_", offs);
      }

      // add the test itself now
      cases += "\n";
      cases += test;
      cases += "\n";

      bool store_out = true;

      if(test.find("%_out_float4") != std::string::npos)
      {
        // if the test outputted a float4, we can dump it directly
        cases += fmt::format("OpStore %Color %_out_float4_{}\n", i);
      }
      else
      {
        // otherwise convert and up-swizzle to float4 as needed
        if(test.find("%_out_float_") != std::string::npos)
        {
          cases += fmt::format(
              "%Color_{0} = OpCompositeConstruct %float4 "
              " %_out_float_{0} %_out_float_{0} %_out_float_{0} %_out_float_{0}\n",
              i);
        }
        else if(test.find("%_out_float2_") != std::string::npos)
        {
          cases += fmt::format(
              "%Color_{0} = OpVectorShuffle %float4 %_out_float2_{0} %_out_float2_{0} 0 1 0 1\n", i);
        }
        else if(test.find("%_out_float3_") != std::string::npos)
        {
          cases += fmt::format(
              "%Color_{0} = OpVectorShuffle %float4 %_out_float3_{0} %_out_float3_{0} 0 1 2 0\n", i);
        }
        else if(test.find("%_out_double_") != std::string::npos)
        {
          cases += fmt::format(
              "%_out_float_{0} = OpFConvert %float %_out_double_{0}\n"
              "%Color_{0} = OpCompositeConstruct %float4 "
              " %_out_float_{0} %_out_float_{0} %_out_float_{0} %_out_float_{0}\n",
              i);
        }
        else if(test.find("%_out_double2_") != std::string::npos)
        {
          cases += fmt::format(
              "%_out_float2_{0} = OpFConvert %float2 %_out_double2_{0}\n"
              "%Color_{0} = OpVectorShuffle %float4 %_out_float2_{0} %_out_float2_{0} 0 1 0 1\n",
              i);
        }
        else if(test.find("%_out_double3_") != std::string::npos)
        {
          cases += fmt::format(
              "%_out_float3_{0} = OpFConvert %float3 %_out_double3_{0}\n"
              "%Color_{0} = OpVectorShuffle %float4 %_out_float3_{0} %_out_float3_{0} 0 1 2 0\n",
              i);
        }
        else if(test.find("%_out_double4_") != std::string::npos)
        {
          cases += fmt::format("%Color_{0} = OpFConvert %float4 %_out_double4_{0}\n", i);
        }
        else if(test.find("%_out_int_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertSToF %float %_out_int_{0}\n"
              "%Color_{0} = OpCompositeConstruct %float4 %_f_{0} %_f_{0} %_f_{0} %_f_{0}\n",
              i);
        }
        else if(test.find("%_out_int2_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertSToF %float2 %_out_int2_{0}\n"
              "%Color_{0} = OpVectorShuffle %float4 %_f_{0} %_f_{0} 0 1 0 1\n",
              i);
        }
        else if(test.find("%_out_int3_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertSToF %float3 %_out_int3_{0}\n"
              "%Color_{0} = OpVectorShuffle %float4 %_f_{0} %_f_{0} 0 1 2 0\n",
              i);
        }
        else if(test.find("%_out_int4_") != std::string::npos)
        {
          cases += fmt::format("%Color_{0} = OpConvertSToF %float4 %_out_int4_{0}\n", i);
        }
        else if(test.find("%_out_uint_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertUToF %float %_out_uint_{0}\n"
              "%Color_{0} = OpCompositeConstruct %float4 %_f_{0} %_f_{0} %_f_{0} %_f_{0}\n",
              i);
        }
        else if(test.find("%_out_uint2_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertUToF %float2 %_out_uint2_{0}\n"
              "%Color_{0} = OpVectorShuffle %float4 %_f_{0} %_f_{0} 0 1 0 1\n",
              i);
        }
        else if(test.find("%_out_uint3_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertUToF %float3 %_out_uint3_{0}\n"
              "%Color_{0} = OpVectorShuffle %float4 %_f_{0} %_f_{0} 0 1 2 0\n",
              i);
        }
        else if(test.find("%_out_uint4_") != std::string::npos)
        {
          cases += fmt::format("%Color_{0} = OpConvertUToF %float4 %_out_uint4_{0}\n", i);
        }
        else if(test.find("%_out_i64_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertSToF %float %_out_i64_{0}\n"
              "%Color_{0} = OpCompositeConstruct %float4 %_f_{0} %_f_{0} %_f_{0} "
              "%_f_{0}\n",
              i);
        }
        else if(test.find("%_out_u64_") != std::string::npos)
        {
          cases += fmt::format(
              "%_f_{0} = OpConvertUToF %float %_out_u64_{0}\n"
              "%Color_{0} = OpCompositeConstruct %float4 %_f_{0} %_f_{0} %_f_{0} "
              "%_f_{0}\n",
              i);
        }
        else if(test.find("; no_out") != std::string::npos)
        {
          store_out = false;
        }
        else
        {
          TEST_FATAL("Test with no recognised output");
        }

        if(store_out)
          cases += fmt::format("OpStore %Color %Color_{}\n", i);
      }

      cases += "OpBranch %break\n";
    }

    if(features.shaderFloat64)
    {
      typesConstants +=
          "%double = OpTypeFloat 64\n"
          "%double2 = OpTypeVector %double 2\n"
          "%double3 = OpTypeVector %double 3\n"
          "%double4 = OpTypeVector %double 4\n"
          "%double2x2 = OpTypeMatrix %double2 2\n"
          "%double3x3 = OpTypeMatrix %double3 3\n"
          "%double2x4 = OpTypeMatrix %double2 4\n"
          "%double4x2 = OpTypeMatrix %double4 2\n"
          "%double4x4 = OpTypeMatrix %double4 4\n";

      typesConstants += "%ptr_Uniform_double = OpTypePointer Uniform %double\n";
      capabilities += "OpCapability Float64\n";
    }

    if(float16Int8Features.shaderFloat16)
    {
      typesConstants += "%half = OpTypeFloat 16\n";
      capabilities += "OpCapability Float16\n";
    }

    if(float16Int8Features.shaderInt8 || storage8Features.storageBuffer8BitAccess ||
       storage8Features.uniformAndStorageBuffer8BitAccess || storage8Features.storagePushConstant8)
    {
      typesConstants +=
          "%i8 = OpTypeInt 8 1\n"
          "%u8 = OpTypeInt 8 0\n";
      capabilities += "OpCapability Int8\n";
    }

    if(features.shaderInt64)
    {
      typesConstants +=
          "%i64 = OpTypeInt 64 1\n"
          "%u64 = OpTypeInt 64 0\n";
      capabilities += "OpCapability Int64\n";
    }

    if(features.shaderInt16 || storage16Features.storageBuffer16BitAccess ||
       storage16Features.uniformAndStorageBuffer16BitAccess ||
       storage16Features.storagePushConstant16 || storage16Features.storageInputOutput16)
    {
      typesConstants +=
          "%i16 = OpTypeInt 16 1\n"
          "%u16 = OpTypeInt 16 0\n";
      capabilities += "OpCapability Int16\n";
    }

    std::string cbuffer =
        "%cbuffer_struct = OpTypeStruct %float4 %float4 %float4 %float4 %float4 %float4 %float4 "
        "                               %float4 %float4 %float4 %float4 %float4 %uint %uint %uint "
        "                               %uint %uint2";

    if(features.shaderFloat64)
      cbuffer += " %double";
    else
      cbuffer += " %uint2";

    cbuffer += "\n";

    typesConstants += cbuffer;
    decorations += R"EOSHADER(

OpDecorate %cbuffer_struct Block
OpDecorate %cbuffer DescriptorSet 0
OpDecorate %cbuffer Binding 10
OpMemberDecorate %cbuffer_struct 0 Offset 0       ; vec4 first
OpMemberDecorate %cbuffer_struct 1 Offset 16      ; vec4 pad1
OpMemberDecorate %cbuffer_struct 2 Offset 32      ; vec4 second
OpMemberDecorate %cbuffer_struct 3 Offset 48      ; vec4 nan
OpMemberDecorate %cbuffer_struct 4 Offset 64      ; vec4 third
OpMemberDecorate %cbuffer_struct 5 Offset 80      ; vec4 pad3
OpMemberDecorate %cbuffer_struct 6 Offset 96      ; vec4 fourth
OpMemberDecorate %cbuffer_struct 7 Offset 112     ; vec4 unorm2PackSource
OpMemberDecorate %cbuffer_struct 8 Offset 128     ; vec4 snorm2PackSource
OpMemberDecorate %cbuffer_struct 9 Offset 144     ; vec4 unorm4PackSource
OpMemberDecorate %cbuffer_struct 10 Offset 160    ; vec4 snorm4PackSource
OpMemberDecorate %cbuffer_struct 11 Offset 176    ; vec4 halfPackSource
OpMemberDecorate %cbuffer_struct 12 Offset 192    ; uint unormUnpackSource
OpMemberDecorate %cbuffer_struct 13 Offset 196    ; uint snormUnpackSource
OpMemberDecorate %cbuffer_struct 14 Offset 200    ; uint halfUnpackSource
OpMemberDecorate %cbuffer_struct 15 Offset 204    ; uint pad
OpMemberDecorate %cbuffer_struct 16 Offset 208    ; uint2 doubleUnpackSource
OpMemberDecorate %cbuffer_struct 17 Offset 216    ; double doublePackSource
)EOSHADER";

    typesConstants +=
        "%ptr_Uniform_cbuffer_struct = OpTypePointer Uniform %cbuffer_struct\n"
        "%cbuffer = OpVariable %ptr_Uniform_cbuffer_struct Uniform\n";

    // now generate all the constants

    for(const std::string &n : null_constants)
      typesConstants += fmt::format("%null_{0} = OpConstantNull %{0}\n", n);

    typesConstants += "\n";

    for(float f : float_constants)
    {
      std::string name = fmt::format("{}", f);
      for(char &c : name)
        if(c == '.')
          c = '_';
      typesConstants += fmt::format("%float_{} = OpConstant %float {}\n", name, f);
      typesConstants += fmt::format("%float_neg{} = OpConstant %float -{}\n", name, f);

      if(features.shaderFloat64)
      {
        typesConstants += fmt::format("%double_{} = OpConstant %double {}\n", name, f);
        typesConstants += fmt::format("%double_neg{} = OpConstant %double -{}\n", name, f);
      }
    }

    typesConstants += "\n";

    for(int32_t i : int_constants)
    {
      typesConstants += fmt::format("%int_{0} = OpConstant %int {0}\n", i);
      typesConstants += fmt::format("%int_neg{0} = OpConstant %int -{0}\n", i);
    }

    typesConstants += "\n";

    for(uint32_t u : uint_constants)
      typesConstants += fmt::format("%uint_{0} = OpConstant %uint {0}\n", u);

    typesConstants += "\n";

    if(features.shaderInt64)
    {
      for(int64_t i : i64_constants)
      {
        typesConstants += fmt::format("%i64_{0} = OpConstant %i64 {0}\n", i);
        typesConstants += fmt::format("%i64_neg{0} = OpConstant %i64 -{0}\n", i);
      }

      typesConstants += "\n";

      for(uint64_t u : u64_constants)
      {
        typesConstants += fmt::format("%u64_{0} = OpConstant %u64 {0}\n", u);
      }

      typesConstants += "\n";
    }
    else
    {
      if(!i64_constants.empty())
        TEST_FATAL("Test using i64 constants without shaderInt64 capability");
      if(!u64_constants.empty())
        TEST_FATAL("Test using u64 constants without shaderInt64 capability");
    }

    for(size_t i = 0; i < 32; i++)
      typesConstants += fmt::format("%randf_{} = OpConstant %float {:.3}\n", i, RANDF(0.0f, 1.0f));

    typesConstants += "\n";

    // vector constants here manually, as we can't pull these out easily
    typesConstants += R"EOSHADER(

 %float4_0000 = OpConstantComposite %float4 %float_0_0 %float_0_0 %float_0_0 %float_0_0
 %float4_1234 = OpConstantComposite %float4 %float_1_0 %float_2_0 %float_3_0 %float_4_0

 %float3_000 = OpConstantComposite %float3 %float_0_0 %float_0_0 %float_0_0
 %float3_123 = OpConstantComposite %float3 %float_1_0 %float_2_0 %float_3_0

 %float2_00 = OpConstantComposite %float2 %float_0_0 %float_0_0
 %float2_12 = OpConstantComposite %float2 %float_1_0 %float_2_0

  %priv_float4_init = OpVariable %ptr_Private_float4 Private %float4_1234

)EOSHADER";

    std::string ret = capabilities + spv_extensions + extinstimport +
                      R"EOSHADER(
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %flatData %linearData %Color %gl_FragCoord
)EOSHADER" + executionmodes +
                      spv_debug + decorations + typesConstants + functions +
                      R"EOSHADER(
       %main = OpFunction %void None %mainfunc
 %main_begin = OpLabel
   %test_ptr = OpAccessChain %ptr_Input_uint %flatData %flatv2f_test_idx
       %test = OpLoad %uint %test_ptr

%zeroVal_ptr = OpAccessChain %ptr_Input_float2 %linearData %v2f_zeroVal_idx
    %zeroVal = OpLoad %float2 %zeroVal_ptr
  %zeroVal_x = OpCompositeExtract %float %zeroVal 0
  %zeroVal_y = OpCompositeExtract %float %zeroVal 1
      %zerof = OpCompositeExtract %float %zeroVal 0

  %inpos_ptr = OpAccessChain %ptr_Input_float2 %linearData %v2f_inpos_idx
      %inpos = OpLoad %float2 %inpos_ptr

  %inposIncreased_ptr = OpAccessChain %ptr_Input_float2 %linearData %v2f_inposIncreased_idx
      %inposIncreased = OpLoad %float2 %inposIncreased_ptr

  %tinyVal_ptr = OpAccessChain %ptr_Input_float %linearData %v2f_tinyVal_idx
      %tinyVal = OpLoad %float %tinyVal_ptr

  %oneVal_ptr = OpAccessChain %ptr_Input_float %linearData %v2f_oneVal_idx
      %oneVal = OpLoad %float %oneVal_ptr

  %negoneVal_ptr = OpAccessChain %ptr_Input_float %linearData %v2f_negoneVal_idx
      %negoneVal = OpLoad %float %negoneVal_ptr

   %posinf = OpFDiv %float %oneVal %zerof
   %neginf = OpFDiv %float %negoneVal %zerof

 ; NaN generation is hard and we want to avoid compilers compiling it out, so generate
 ; it in shader and multiply by one from a UBO so we get NaN either way
 ; (since NaN * anything = NaN)
 %nan_shad = OpFDiv %float %zerof %zerof

  %nan_ptr = OpAccessChain %ptr_Uniform_float4 %cbuffer %uint_3
  %nan_ubo = OpLoad %float4 %nan_ptr
%nan_ubo_x = OpCompositeExtract %float %nan_ubo 0
      %nan = OpFMul %float %nan_shad %nan_ubo_x

%intval_ptr = OpAccessChain %ptr_Input_uint %flatData %flatv2f_intval_idx
    %intval = OpLoad %uint %intval_ptr
       %tmp = OpISub %uint %intval %test
     %zerou = OpISub %uint %tmp %int_7
     %zeroi = OpBitcast %int %zerou

)EOSHADER";

    if(features.shaderFloat64)
      ret += "%zerof64 = OpFConvert %double %zerof\n";

    // generate dynamic versions of the constants
    for(float f : float_constants)
    {
      std::string name = fmt::format("{}", f);
      for(char &c : name)
        if(c == '.')
          c = '_';
      ret += fmt::format("%float_dyn_{0} = OpFAdd %float %zerof %float_{0}\n", name);
      ret += fmt::format("%float_dyn_neg{0} = OpFAdd %float %zerof %float_neg{0}\n", name);

      if(features.shaderFloat64)
      {
        ret += fmt::format("%double_dyn_{0} = OpFAdd %double %zerof64 %double_{0}\n", name);
        ret += fmt::format("%double_dyn_neg{0} = OpFAdd %double %zerof64 %double_neg{0}\n", name);
      }
    }

    ret += "\n";

    for(int32_t i : int_constants)
    {
      ret += fmt::format("%int_dyn_{0} = OpIAdd %int %zeroi %int_{0}\n", i);
      ret += fmt::format("%int_dyn_neg{0} = OpIAdd %int %zeroi %int_neg{0}\n", i);
    }

    ret += "\n";

    for(uint32_t u : uint_constants)
      ret += fmt::format("%uint_dyn_{0} = OpIAdd %uint %zerou %uint_{0}\n", u);

    ret += "\n";

    for(size_t i = 0; i < 32; i++)
      ret += fmt::format("%randf_dyn_{0} = OpFAdd %float %zerof %randf_{0}\n", i);

    ret += "\n";

    ret += R"EOSHADER(

 %float4_dyn_0000 = OpCompositeConstruct %float4 %float_dyn_0_0 %float_dyn_0_0 %float_dyn_0_0 %float_dyn_0_0
 %float4_dyn_1234 = OpCompositeConstruct %float4 %float_dyn_1_0 %float_dyn_2_0 %float_dyn_3_0 %float_dyn_4_0

 %float3_dyn_000 = OpCompositeConstruct %float3 %float_dyn_0_0 %float_dyn_0_0 %float_dyn_0_0
 %float3_dyn_123 = OpCompositeConstruct %float3 %float_dyn_1_0 %float_dyn_2_0 %float_dyn_3_0

 %float2_dyn_00 = OpCompositeConstruct %float2 %float_dyn_0_0 %float_dyn_0_0
 %float2_dyn_12 = OpCompositeConstruct %float2 %float_dyn_1_0 %float_dyn_2_0

)EOSHADER";

    ret += switch_str;
    ret += cases;

    ret += R"EOSHADER(

    %default = OpLabel
               OpStore %Color %float4_0000
               OpBranch %break

      %break = OpLabel
               OpReturn
               OpFunctionEnd
)EOSHADER";

    return ret;
  }

  uint32_t vk_version = 0x10;

  VkPhysicalDevice16BitStorageFeaturesKHR storage16Features = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,
  };
  VkPhysicalDevice8BitStorageFeaturesKHR storage8Features = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
  };
  VkPhysicalDeviceFloat16Int8FeaturesKHR float16Int8Features = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
  };

  void Prepare(int argc, char **argv)
  {
    // require descriptor indexing
    optDevExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    // dependencies of VK_EXT_descriptor_indexing
    optDevExts.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);

    // add float16/int8 extensions
    optDevExts.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);

    // dependencies of VK_KHR_8bit_storage
    optDevExts.push_back(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);

    // we require this to pixel shader debug anyway, so we might as well require it for all tests.
    features.fragmentStoresAndAtomics = VK_TRUE;

    // this is so widely supported just require it without fallback
    features.imageCubeArray = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    const bool descIndexing = std::find(devExts.begin(), devExts.end(),
                                        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) != devExts.end();
    const bool storage16 = std::find(devExts.begin(), devExts.end(),
                                     VK_KHR_16BIT_STORAGE_EXTENSION_NAME) != devExts.end();
    const bool storage8 = std::find(devExts.begin(), devExts.end(),
                                    VK_KHR_8BIT_STORAGE_EXTENSION_NAME) != devExts.end();
    const bool float16int8 = std::find(devExts.begin(), devExts.end(),
                                       VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME) != devExts.end();

    vk_version = 0x10;

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
      vk_version = 0x11;

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
      vk_version = 0x12;

#define LIMIT_CHECK(limit, req)                                                     \
  if(physProperties.limits.limit < req)                                             \
    Avail = fmt::format("Limit '" #limit "' {} is insufficient (need at least {})", \
                        physProperties.limits.limit, req);

    if(descIndexing)
    {
      LIMIT_CHECK(maxPerStageDescriptorSampledImages, 128);
      LIMIT_CHECK(maxPerStageDescriptorSamplers, 64);
      LIMIT_CHECK(maxPerStageDescriptorStorageBuffers, 16);
      LIMIT_CHECK(maxPerStageDescriptorStorageImages, 64);
    }

    // enable features we can optionally test with.
    VkPhysicalDeviceFeatures supported;
    vkGetPhysicalDeviceFeatures(phys, &supported);

    if(supported.shaderFloat64)
      features.shaderFloat64 = VK_TRUE;
    if(supported.shaderInt64)
      features.shaderInt64 = VK_TRUE;
    if(supported.shaderInt16)
      features.shaderInt16 = VK_TRUE;

    if(descIndexing)
    {
      static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingFeatures = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
      };

      getPhysFeatures2(&descIndexingFeatures);

      // enable descriptor indexing on arrays of all types

      if(!descIndexingFeatures.runtimeDescriptorArray)
        Avail = "Descriptor indexing feature 'runtimeDescriptorArray' not available";
      else if(!descIndexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing)
        Avail =
            "Descriptor indexing feature 'shaderUniformTexelBufferArrayDynamicIndexing' not "
            "available";
      else if(!descIndexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing)
        Avail =
            "Descriptor indexing feature 'shaderStorageTexelBufferArrayDynamicIndexing' not "
            "available";
      else if(!descIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing)
        Avail =
            "Descriptor indexing feature 'shaderUniformBufferArrayNonUniformIndexing' not "
            "available";
      else if(!descIndexingFeatures.shaderSampledImageArrayNonUniformIndexing)
        Avail =
            "Descriptor indexing feature 'shaderSampledImageArrayNonUniformIndexing' not available";
      else if(!descIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing)
        Avail =
            "Descriptor indexing feature 'shaderStorageBufferArrayNonUniformIndexing' not "
            "available";
      else if(!descIndexingFeatures.shaderStorageImageArrayNonUniformIndexing)
        Avail =
            "Descriptor indexing feature 'shaderStorageImageArrayNonUniformIndexing' not available";
      else if(!descIndexingFeatures.shaderUniformTexelBufferArrayNonUniformIndexing)
        Avail =
            "Descriptor indexing feature 'shaderUniformTexelBufferArrayNonUniformIndexing' not "
            "available";
      else if(!descIndexingFeatures.shaderStorageTexelBufferArrayNonUniformIndexing)
        Avail =
            "Descriptor indexing feature 'shaderStorageTexelBufferArrayNonUniformIndexing' not "
            "available";

      devInfoNext = &descIndexingFeatures;
    }

    if(storage16)
    {
      // enable all available features
      getPhysFeatures2(&storage16Features);

      storage16Features.pNext = (void *)devInfoNext;
      devInfoNext = &storage16Features;
    }

    if(storage8)
    {
      // enable all available features
      getPhysFeatures2(&storage8Features);

      storage8Features.pNext = (void *)devInfoNext;
      devInfoNext = &storage8Features;
    }

    if(float16int8)
    {
      // enable all available features
      getPhysFeatures2(&float16Int8Features);

      float16Int8Features.pNext = (void *)devInfoNext;
      devInfoNext = &float16Int8Features;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    make_asm_tests();

    const bool descIndexing = std::find(devExts.begin(), devExts.end(),
                                        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) != devExts.end();
    const bool storage16 = std::find(devExts.begin(), devExts.end(),
                                     VK_KHR_16BIT_STORAGE_EXTENSION_NAME) != devExts.end();
    const bool storage8 = std::find(devExts.begin(), devExts.end(),
                                    VK_KHR_8BIT_STORAGE_EXTENSION_NAME) != devExts.end();
    const bool float16int8 = std::find(devExts.begin(), devExts.end(),
                                       VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME) != devExts.end();

    if(storage16)
      TEST_LOG("Running tests on 16-bit storage");

    if(storage8)
      TEST_LOG("Running tests on 8-bit storage");

    if(float16int8)
      TEST_LOG("Running tests on half and int8 arithmetic");

    if(features.shaderFloat64)
      TEST_LOG("Running tests on doubles");

    if(features.shaderInt64)
      TEST_LOG("Running tests on int64");

    if(features.shaderInt16)
      TEST_LOG("Running tests on int16 arithmetic");

    pixel_glsl1.replace(pixel_glsl1.find("#define TEST_DESC_INDEXING"),
                        sizeof("#define TEST_DESC_INDEXING"),
                        fmt::format("#define TEST_DESC_INDEXING {}", descIndexing ? 1 : 0));

    pixel_glsl2.replace(pixel_glsl2.find("#define TEST_DESC_INDEXING"),
                        sizeof("#define TEST_DESC_INDEXING"),
                        fmt::format("#define TEST_DESC_INDEXING {}", descIndexing ? 1 : 0));

    size_t lastTest = pixel_glsl1.rfind("case ");
    lastTest += sizeof("case ") - 1;

    const uint32_t numGLSL1Tests = atoi(pixel_glsl1.c_str() + lastTest) + 1;

    lastTest = pixel_glsl2.rfind("case ");
    lastTest += sizeof("case ") - 1;

    const uint32_t numGLSL2Tests = atoi(pixel_glsl2.c_str() + lastTest) + 1;

    const uint32_t numASMTests = (uint32_t)asm_tests.size();

    VkDescriptorSetLayout setlayout0 = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {11, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {12, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {13, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {16, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {17, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {18, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {19, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {21, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {22, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {30, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {31, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {32, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    std::vector<VkDescriptorSetLayout> setLayouts = {setlayout0};

    // this set layout has arrays of each type. We'll uniformly, dynamic-uniformly, and
    // non-uniformly access each of these
    VkDescriptorSetLayout setlayout1 = VK_NULL_HANDLE;
    VkDescriptorSetLayout setlayout2 = VK_NULL_HANDLE;

    if(descIndexing)
    {
      setlayout1 = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
          {1, VK_DESCRIPTOR_TYPE_SAMPLER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {2, VK_DESCRIPTOR_TYPE_SAMPLER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {7, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {8, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {9, VK_DESCRIPTOR_TYPE_SAMPLER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
          {21, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 14, VK_SHADER_STAGE_FRAGMENT_BIT},
      }));

      setlayout2 = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
          {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {9, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

          {10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {15, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {19, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

          {20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {21, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {22, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {23, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {24, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {25, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {26, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {27, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {28, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {29, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

          {30, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {31, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {32, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {33, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {34, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {35, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {36, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {37, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {38, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {39, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

          {40, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {41, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {42, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {43, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {44, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {45, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {46, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {47, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {48, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {49, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

          {50, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {51, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {52, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {53, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {54, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {55, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {56, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {57, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {58, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {59, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      }));

      setLayouts.push_back(setlayout1);
      setLayouts.push_back(setlayout2);
    }

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        setLayouts, {
                        vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(Vec4i)),
                    }));

    // calculate number of tests, wrapping each row at 256
    uint32_t texWidth = AlignUp(std::max(std::max(numGLSL1Tests, numGLSL2Tests), numASMTests), 256U);
    uint32_t texHeight = std::max(1U, texWidth / 256U);
    texWidth /= texHeight;

    // 4x4 for each test
    texWidth *= 4;
    texHeight *= 4;

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(texWidth, texHeight, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer =
        createFramebuffer(vkh::FramebufferCreateInfo(renderPass, {imgview}, {texWidth, texHeight}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, ConstsA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, ConstsA2V, pos), vkh::vertexAttr(1, 0, ConstsA2V, zero),
        vkh::vertexAttr(2, 0, ConstsA2V, one), vkh::vertexAttr(3, 0, ConstsA2V, negone),
        vkh::vertexAttr(4, 0, ConstsA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel_glsl1, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline glslpipe1 = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex2, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel_glsl2, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline glslpipe2 = createGraphicsPipeline(pipeCreateInfo);

    SPIRVTarget target = SPIRVTarget::vulkan;

    if(vk_version >= 0x11)
      target = SPIRVTarget::vulkan11;
    if(vk_version >= 0x12)
      target = SPIRVTarget::vulkan12;

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(make_pixel_asm(), ShaderLang::spvasm, ShaderStage::frag, "main", {},
                            target),
    };

    VkPipeline asmpipe = createGraphicsPipeline(pipeCreateInfo);

    float triWidth = 8.0f / float(texWidth);
    float triHeight = 8.0f / float(texHeight);

    ConstsA2V triangle[] = {
        {Vec4f(-1.0f, -1.0f, triWidth, triHeight), 0.0f, 1.0f, -1.0f, Vec2f(0.0f, 0.0f)},
        {Vec4f(-1.0f + triWidth, -1.0f, triWidth, triHeight), 0.0f, 1.0f, -1.0f, Vec2f(1.0f, 0.0f)},
        {Vec4f(-1.0f, -1.0f + triHeight, triWidth, triHeight), 0.0f, 1.0f, -1.0f, Vec2f(0.0f, 1.0f)},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(triangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(triangle);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    AllocatedImage queryTest(this,
                             vkh::ImageCreateInfo(183, 347, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                                  VK_IMAGE_USAGE_SAMPLED_BIT, 4, 3),
                             VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView queryTestView = createImageView(vkh::ImageViewCreateInfo(
        queryTest.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedImage queryTestMS(
        this,
        vkh::ImageCreateInfo(183, 347, 0, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, 1,
                             5, VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView queryTestMSView = createImageView(vkh::ImageViewCreateInfo(
        queryTestMS.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedImage smiley(
        this,
        vkh::ImageCreateInfo(rgba8.width, rgba8.height, 0, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView smileyview = createImageView(
        vkh::ImageViewCreateInfo(smiley.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM));
    AllocatedBuffer uploadBuf(this,
                              vkh::BufferCreateInfo(rgba8.data.size() * sizeof(uint32_t),
                                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                              VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    AllocatedImage shadowimg(this,
                             vkh::ImageCreateInfo(16, 16, 0, VK_FORMAT_D32_SFLOAT,
                                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                      VK_IMAGE_USAGE_SAMPLED_BIT),
                             VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView shadowview = createImageView(
        vkh::ImageViewCreateInfo(shadowimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, {},
                                 vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)));

    uploadBuf.upload(rgba8.data.data(), rgba8.data.size() * sizeof(uint32_t));

    std::vector<byte> typeData;
    typeData.resize(sizeof(Vec4f) * 16 * 16 * 32 * 3);

    uint32_t typeOffset[] = {
        // float data
        sizeof(Vec4f) * 16 * 16 * 32 * 0,
        // uint data
        sizeof(Vec4f) * 16 * 16 * 32 * 1,
        // int data
        sizeof(Vec4f) * 16 * 16 * 32 * 2,
    };

    for(size_t typeVariant = 0; typeVariant < 3; typeVariant++)
    {
      byte *dst = typeData.data() + typeOffset[typeVariant];
      union
      {
        float f[4];
        int i[4];
      } rnd;
      memset(&rnd, 0, sizeof(rnd));

      for(size_t x = 0; x < 16; x++)
      {
        for(size_t y = 0; y < 16; y++)
        {
          for(size_t z = 0; z < 32; z++)
          {
            if(typeVariant == 0)
            {
              rnd.f[0] = RANDF(-10.0f, 10.0f);
              rnd.f[1] = RANDF(-10.0f, 10.0f);
              rnd.f[2] = RANDF(-10.0f, 10.0f);
              rnd.f[3] = RANDF(-10.0f, 10.0f);
            }
            else if(typeVariant == 1)
            {
              rnd.i[0] = (int32_t)RANDF(100.0f, 500.0f);
              rnd.i[1] = (int32_t)RANDF(100.0f, 500.0f);
              rnd.i[2] = (int32_t)RANDF(100.0f, 500.0f);
              rnd.i[3] = (int32_t)RANDF(100.0f, 500.0f);
            }
            else if(typeVariant == 2)
            {
              rnd.i[0] = (int32_t)RANDF(-200.0f, 200.0f);
              rnd.i[1] = (int32_t)RANDF(-200.0f, 200.0f);
              rnd.i[2] = (int32_t)RANDF(-200.0f, 200.0f);
              rnd.i[3] = (int32_t)RANDF(-200.0f, 200.0f);
            }
            memcpy(dst, &rnd.f, sizeof(Vec4f));
          }
        }
      }
    }

    AllocatedBuffer typeDataBuf(
        this, vkh::BufferCreateInfo(typeData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    typeDataBuf.upload(typeData.data(), typeData.size());

    AllocatedImage randomcube(
        this,
        vkh::ImageCreateInfo(rgba8.width, rgba8.height, 0, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1, 6,
                             VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView randomcubeview = createImageView(vkh::ImageViewCreateInfo(
        randomcube.image, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_UNORM));

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, smiley.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, randomcube.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, queryTest.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, queryTestMS.image),
          });

      VkBufferImageCopy copy = {};
      copy.imageExtent = {rgba8.width, rgba8.height, 1};
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.layerCount = 1;

      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, smiley.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      for(uint32_t i = 0; i < 6; i++)
      {
        copy.imageSubresource.baseArrayLayer = i;
        vkCmdCopyBufferToImage(cmd, typeDataBuf.buffer, randomcube.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      }

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, smiley.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, randomcube.image),
          });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});

      vkDeviceWaitIdle(device);
    }

    VkSampler pointsampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_NEAREST));
    VkSampler linearsampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
    VkSampler mipsampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
    VkSampler shadowsampler = createSampler(vkh::SamplerCreateInfo(
        VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f, 0.0f, 0.0f, VK_COMPARE_OP_LESS_OR_EQUAL));

    VkDescriptorSet descset0 = allocateDescriptorSet(setlayout0);
    VkDescriptorSet descset1 = VK_NULL_HANDLE;
    VkDescriptorSet descset2 = VK_NULL_HANDLE;

    if(descIndexing)
    {
      descset1 = allocateDescriptorSet(setlayout1);
      descset2 = allocateDescriptorSet(setlayout2);
    }

    Vec4f cbufferdata[64] = {};

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cbufferdata[1] = Vec4f(1.1f, 2.2f, 3.3f, 4.4f);
    cbufferdata[2] = Vec4f(5.5f, 6.6f, 7.7f, 8.8f);
    cbufferdata[3] = Vec4f(std::numeric_limits<float>::quiet_NaN());
    cbufferdata[4] = Vec4f(9.9f, 9.99f, 9.999f, 9.999f);
    cbufferdata[6] = Vec4f(100.0f, 200.0f, 300.0f, 400.0f);

    // unorm2PackSource
    cbufferdata[7] = Vec4f(99.0f, 28099.0f / 65535.0f, 0.0f, 0.0f);
    // snorm2PackSource
    cbufferdata[8] = Vec4f(99.0f, -28099.0f / 32767.0f, 0.0f, 0.0f);
    // unorm4PackSource
    cbufferdata[9] = Vec4f(99.0f, 28.0f / 255.0f, 99.0f / 255.0f, 182.0f / 255.0f);
    // snorm4PackSource
    cbufferdata[10] = Vec4f(99.0f, -28.0f / 127.0f, 99.0f / 127.0f, -102.0f / 127.0f);
    // halfPackSource - we pick exact half values to avoid rounding problems
    cbufferdata[11] = Vec4f(98.125f, 76.375f, 54.5625f, 32.78125f);

    uint32_t index = 4;
    memcpy(&cbufferdata[1], &index, sizeof(index));

    Vec4u unpack = {};

    // unormUnpackSource
    unpack.x = 0xf0dd103c;
    // snormUnpackSource
    unpack.y = 0xf0dd103c;
    // halfUnpackSource
    unpack.z = (uint32_t(MakeHalf(81.5f)) << 16) | MakeHalf(101.03f);

    // unpack sources
    memcpy(&cbufferdata[12], &unpack, sizeof(unpack));

    double unpackDouble = 3.1415926535;
    memcpy(&cbufferdata[13].x, &unpackDouble, sizeof(unpackDouble));
    memcpy(&cbufferdata[14].z, &unpackDouble, sizeof(unpackDouble));

    // move to account for offset
    memmove(&cbufferdata[16], &cbufferdata[0], sizeof(Vec4f) * 16);
    memset(&cbufferdata[0], 0, sizeof(Vec4f) * 16);

    cb.upload(cbufferdata);

    AllocatedBuffer texbuffer(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    texbuffer.upload(cbufferdata);

    AllocatedBuffer store_buffer(
        this,
        vkh::BufferCreateInfo(1024 * sizeof(Vec4f), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedBuffer atomic_buffer(
        this,
        vkh::BufferCreateInfo(texWidth * texHeight * sizeof(Vec4f),
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedBuffer store_texbuffer(
        this,
        vkh::BufferCreateInfo(1024 * sizeof(Vec4f), VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedImage store_image(
        this,
        vkh::ImageCreateInfo(128, 128, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    VkImageView store_view = createImageView(vkh::ImageViewCreateInfo(
        store_image.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedImage atomic_image(
        this,
        vkh::ImageCreateInfo(texWidth, texHeight, 0, VK_FORMAT_R32_UINT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    VkImageView atomic_view = createImageView(
        vkh::ImageViewCreateInfo(atomic_image.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT));

    VkBufferView bufview =
        createBufferView(vkh::BufferViewCreateInfo(texbuffer.buffer, VK_FORMAT_R32G32B32A32_SFLOAT));
    VkBufferView store_bufview = createBufferView(
        vkh::BufferViewCreateInfo(store_texbuffer.buffer, VK_FORMAT_R32G32B32A32_SFLOAT));

    setName(pointsampler, "pointsampler");
    setName(linearsampler, "linearsampler");
    setName(mipsampler, "mipsampler");
    setName(queryTest.image, "queryTest");
    setName(queryTestMS.image, "queryTestMS");
    setName(smiley.image, "smiley");
    setName(texbuffer.buffer, "texbuffer");
    setName(store_buffer.buffer, "store_buffer");
    setName(atomic_buffer.buffer, "atomic_buffer");
    setName(store_texbuffer.buffer, "store_texbuffer");
    setName(store_image.image, "store_image");
    setName(atomic_image.image, "atomic_image");

    AllocatedImage storezoo_u2D(
        this,
        vkh::ImageCreateInfo(16, 16, 0, VK_FORMAT_R32G32B32A32_UINT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    VkImageView storezoo_u2D_view = createImageView(vkh::ImageViewCreateInfo(
        storezoo_u2D.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_UINT));

    setName(storezoo_u2D.image, "storezoo_u2D");

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(descset0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    {vkh::DescriptorBufferInfo(cb.buffer, 0, sizeof(cbufferdata))}),
            vkh::WriteDescriptorSet(descset0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    {vkh::DescriptorBufferInfo(cb.buffer, 0, sizeof(cbufferdata))}),
            vkh::WriteDescriptorSet(
                descset0, 11, VK_DESCRIPTOR_TYPE_SAMPLER,
                {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, pointsampler)}),
            vkh::WriteDescriptorSet(
                descset0, 12, VK_DESCRIPTOR_TYPE_SAMPLER,
                {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, linearsampler)}),
            vkh::WriteDescriptorSet(
                descset0, 13, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                {vkh::DescriptorImageInfo(smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          VK_NULL_HANDLE)}),
            vkh::WriteDescriptorSet(
                descset0, 14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          linearsampler)}),
            vkh::WriteDescriptorSet(descset0, 15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(store_buffer.buffer)}),
            vkh::WriteDescriptorSet(
                descset0, 16, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                {vkh::DescriptorImageInfo(store_view, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
            vkh::WriteDescriptorSet(descset0, 17, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, {bufview}),
            vkh::WriteDescriptorSet(descset0, 18, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                    {store_bufview}),
            vkh::WriteDescriptorSet(
                descset0, 19, VK_DESCRIPTOR_TYPE_SAMPLER,
                {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, shadowsampler)}),
            vkh::WriteDescriptorSet(
                descset0, 20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(randomcubeview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          linearsampler)}),
            vkh::WriteDescriptorSet(descset0, 21, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(atomic_buffer.buffer)}),
            vkh::WriteDescriptorSet(
                descset0, 22, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                {vkh::DescriptorImageInfo(atomic_view, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),

            vkh::WriteDescriptorSet(
                descset0, 30, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(queryTestView, VK_IMAGE_LAYOUT_GENERAL, mipsampler)}),
            vkh::WriteDescriptorSet(
                descset0, 31, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(queryTestMSView, VK_IMAGE_LAYOUT_GENERAL, mipsampler)}),
            vkh::WriteDescriptorSet(
                descset0, 32, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                {vkh::DescriptorImageInfo(shadowview, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
        });

    if(descIndexing)
    {
      vkh::updateDescriptorSets(
          device, {
                      vkh::WriteDescriptorSet(
                          descset2, 41, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                          {vkh::DescriptorImageInfo(storezoo_u2D_view, VK_IMAGE_LAYOUT_GENERAL,
                                                    VK_NULL_HANDLE)}),
                  });

      for(uint32_t i = 0; i < 14; i++)
      {
        vkh::updateDescriptorSets(
            device,
            {
                vkh::WriteDescriptorSet(descset1, 1, i, VK_DESCRIPTOR_TYPE_SAMPLER,
                                        {vkh::DescriptorImageInfo(
                                            VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, pointsampler)}),
                vkh::WriteDescriptorSet(
                    descset1, 2, i, VK_DESCRIPTOR_TYPE_SAMPLER,
                    {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED,
                                              linearsampler)}),
                vkh::WriteDescriptorSet(
                    descset1, 3, i, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    {vkh::DescriptorImageInfo(shadowview, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
                vkh::WriteDescriptorSet(
                    descset1, 4, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    {vkh::DescriptorImageInfo(smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                              linearsampler)}),
                vkh::WriteDescriptorSet(descset1, 5, i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        {vkh::DescriptorBufferInfo(store_buffer.buffer)}),
                vkh::WriteDescriptorSet(
                    descset1, 6, i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    {vkh::DescriptorImageInfo(store_view, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
                vkh::WriteDescriptorSet(descset1, 7, i, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                        {bufview}),
                vkh::WriteDescriptorSet(descset1, 8, i, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                        {store_bufview}),
                vkh::WriteDescriptorSet(
                    descset1, 9, i, VK_DESCRIPTOR_TYPE_SAMPLER,
                    {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED,
                                              shadowsampler)}),

                vkh::WriteDescriptorSet(
                    descset1, 20, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    {vkh::DescriptorImageInfo(queryTestView, VK_IMAGE_LAYOUT_GENERAL, mipsampler)}),
                vkh::WriteDescriptorSet(
                    descset1, 21, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    {vkh::DescriptorImageInfo(queryTestMSView, VK_IMAGE_LAYOUT_GENERAL, mipsampler)}),
            });
      }
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, store_image.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, atomic_image.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, storezoo_u2D.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, shadowimg.image,
                                      vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, store_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, atomic_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, store_texbuffer.buffer),
          });

      vkCmdClearDepthStencilImage(cmd, shadowimg.image, VK_IMAGE_LAYOUT_GENERAL,
                                  vkh::ClearDepthStencilValue({0.5f, 0}), 1,
                                  vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT));

      vkCmdClearColorImage(cmd, store_image.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(6.66f, 6.66f, 6.66f, 6.66f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdClearColorImage(cmd, atomic_image.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0x42424242U, 0x42424242U, 0x42424242U, 0x42424242U),
                           1, vkh::ImageSubresourceRange());
      vkCmdClearColorImage(cmd, storezoo_u2D.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(8U, 18U, 28U, 38U), 1, vkh::ImageSubresourceRange());
      vkCmdFillBuffer(cmd, store_buffer.buffer, 0, VK_WHOLE_SIZE, 0x42424242);
      vkCmdFillBuffer(cmd, atomic_buffer.buffer, 0, VK_WHOLE_SIZE, 0x42424242);
      vkCmdFillBuffer(cmd, store_texbuffer.buffer, 0, VK_WHOLE_SIZE, 0);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(
                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, store_image.image),
              vkh::ImageMemoryBarrier(
                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, atomic_image.image),
              vkh::ImageMemoryBarrier(
                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, storezoo_u2D.image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       store_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       atomic_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, store_texbuffer.buffer),
          });

      VkViewport v = {};
      v.maxDepth = 1.0f;
      v.width = (float)texWidth;
      v.height = (float)texHeight;

      VkRect2D s = {};
      s.extent.width = texWidth;
      s.extent.height = texHeight;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe1);
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &s);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      Vec4i push = Vec4i(101, 103, 107, 109);

      std::vector<VkDescriptorSet> descSets = {descset0};

      if(descIndexing)
      {
        descSets.push_back(descset1);
        descSets.push_back(descset2);
      }

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descSets,
                                 {0, sizeof(Vec4f) * 16});
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(Vec4i), &push);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                    {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "GLSL1 tests");
      uint32_t numTests = numGLSL1Tests;
      uint32_t offset = 0;
      // loop drawing 256 tests at a time
      while(numTests > 0)
      {
        uint32_t num = std::min(numTests, 256U);
        vkCmdDraw(cmd, 3, num, 0, offset);
        offset += num;
        numTests -= num;
      }
      popMarker(cmd);

      vkCmdEndRenderPass(cmd);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, asmpipe);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                    {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "ASM tests");
      numTests = numASMTests;
      offset = 0;
      // loop drawing 256 tests at a time
      while(numTests > 0)
      {
        uint32_t num = std::min(numTests, 256U);
        vkCmdDraw(cmd, 3, num, 0, offset);
        offset += num;
        numTests -= num;
      }
      popMarker(cmd);

      vkCmdEndRenderPass(cmd);

      // sync all the storage work
      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                      store_image.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                      atomic_image.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                      storezoo_u2D.image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       store_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       atomic_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       store_texbuffer.buffer),
          });

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe2);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                    {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "GLSL2 tests");
      numTests = numGLSL2Tests;
      offset = 0;
      // loop drawing 256 tests at a time
      while(numTests > 0)
      {
        uint32_t num = std::min(numTests, 256U);
        vkCmdDraw(cmd, 3, num, 0, offset);
        offset += num;
        numTests -= num;
      }
      popMarker(cmd);

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
