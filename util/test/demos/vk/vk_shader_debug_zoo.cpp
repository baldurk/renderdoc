/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

  std::string pixel_glsl = R"EOSHADER(
#version 460 core

#extension GL_EXT_samplerless_texture_functions : require

layout(set = 0, binding = 0, std140) uniform constsbuf
{
  vec4 first;
  vec4 pad1;
  vec4 second;
  vec4 pad2;
  vec4 third;
  vec4 pad3;
  vec4 fourth;
  vec4 pad4;
} cbuf;

layout(set = 0, binding = 1) uniform sampler pointSampler;
layout(set = 0, binding = 2) uniform sampler linearSampler;

layout(set = 0, binding = 3) uniform texture2D sampledImage;

layout(set = 0, binding = 4) uniform sampler2D linearSampledImage;

/*
layout(set = 0, binding = 5, std430) buffer storebuftype
{
  vec4 x;
  uvec4 y;
  vec4 arr[];
} storebuf;
*/

//layout(set = 0, binding = 6, rgba32f) uniform coherent image2D storeImage;

//layout(set = 0, binding = 7, rgba32f) uniform coherent samplerBuffer texBuffer;
//layout(set = 0, binding = 8, rgba32f) uniform coherent imageBuffer storeTexBuffer;

layout(set = 0, binding = 20) uniform sampler2DArray queryTest;
layout(set = 0, binding = 21) uniform sampler2DMSArray queryTestMS;

layout(push_constant) uniform PushData {
  layout(offset = 16) ivec4 data;
} push;

#define inout_type in

)EOSHADER" + v2f + R"EOSHADER(

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  float  posinf = linearData.oneVal/linearData.zeroVal.x;
  float  neginf = linearData.negoneVal/linearData.zeroVal.x;
  float  nan = linearData.zeroVal.x/linearData.zeroVal.y;

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
              cbuf.pad1 + cbuf.pad2 + cbuf.pad3 + cbuf.pad4;
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
    default: break;
  }
}

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
          if(sign)
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
        "%_z = OpConvertFToU %uint %float_dyn_neg1_0\n"
        "%_w = OpConvertFToU %uint %float_dyn_1_3\n"
        "%_out_uint4 = OpCompositeConstruct %uint4 %_x %_y %_z %_w\n",

        "%_x = OpConvertFToU %uint %float_dyn_1_0\n"
        "%_y = OpConvertFToU %uint %float_dyn_1_5\n"
        "%_z = OpConvertFToU %uint %float_dyn_0_5\n"
        "%_w = OpConvertFToU %uint %float_dyn_neg1_5\n"
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

        "%frag = OpLoad %float4 %gl_FragCoord\n"
        "%_out_float4 = OpCopyObject %float4 %frag\n",
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
      offs = test.find("%float_");
      while(offs != std::string::npos)
      {
        offs += 7;    // past %float_

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

        offs = test.find("%float_", offs);
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

      // find any int constants referenced
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

    std::string ret = R"EOSHADER(
               OpCapability Shader
    %glsl450 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %flatData %linearData %Color %gl_FragCoord
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %flatData Flat
               OpDecorate %flatData Location 1
               OpDecorate %linearData Location 3
               OpDecorate %Color Index 0
               OpDecorate %Color Location 0
               OpDecorate %gl_FragCoord BuiltIn FragCoord

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

        %v2f = OpTypeStruct %float2 %float2 %float2 %float %float %float
    %flatv2f = OpTypeStruct %uint %uint

      %child = OpTypeStruct %float4 %float3 %float
     %parent = OpTypeStruct %float4 %child %float4x4

     %f32f32 = OpTypeStruct %float %float
     %f32i32 = OpTypeStruct %float %int

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

  %linearData = OpVariable %ptr_Input_v2f Input
    %flatData = OpVariable %ptr_Input_flatv2f Input
%gl_FragCoord = OpVariable %ptr_Input_float4 Input
       %Color = OpVariable %ptr_Output_float4 Output

    %priv_int = OpVariable %ptr_Private_int Private
  %priv_float = OpVariable %ptr_Private_float Private

       %flatv2f_test_idx = OpConstant %int 0
     %flatv2f_intval_idx = OpConstant %int 1

        %v2f_zeroVal_idx = OpConstant %int 0
          %v2f_inpos_idx = OpConstant %int 1
 %v2f_inposIncreased_idx = OpConstant %int 2
        %v2f_tinyVal_idx = OpConstant %int 3
         %v2f_oneVal_idx = OpConstant %int 4
      %v2f_negoneVal_idx = OpConstant %int 5

)EOSHADER";

    // now generate all the constants

    for(const std::string &n : null_constants)
      ret += fmt::format("%null_{0} = OpConstantNull %{0}\n", n);

    ret += "\n";

    for(float f : float_constants)
    {
      std::string name = fmt::format("{}", f);
      for(char &c : name)
        if(c == '.')
          c = '_';
      ret += fmt::format("%float_{} = OpConstant %float {}\n", name, f);
      ret += fmt::format("%float_neg{} = OpConstant %float -{}\n", name, f);
    }

    ret += "\n";

    for(int32_t i : int_constants)
    {
      ret += fmt::format("%int_{0} = OpConstant %int {0}\n", i);
      ret += fmt::format("%int_neg{0} = OpConstant %int -{0}\n", i);
    }

    ret += "\n";

    for(uint32_t u : uint_constants)
      ret += fmt::format("%uint_{0} = OpConstant %uint {0}\n", u);

    ret += "\n";

    for(size_t i = 0; i < 32; i++)
      ret += fmt::format("%randf_{} = OpConstant %float {:.3}\n", i, RANDF(0.0f, 1.0f));

    ret += "\n";

    // vector constants here manually, as we can't pull these out easily
    ret += R"EOSHADER(

 %float4_0000 = OpConstantComposite %float4 %float_0_0 %float_0_0 %float_0_0 %float_0_0
 %float4_1234 = OpConstantComposite %float4 %float_1_0 %float_2_0 %float_3_0 %float_4_0

)EOSHADER";

    // now generate the entry point, and load the inputs
    ret += R"EOSHADER(
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
      %nan = OpFDiv %float %zerof %zerof

%intval_ptr = OpAccessChain %ptr_Input_uint %flatData %flatv2f_intval_idx
    %intval = OpLoad %uint %intval_ptr
       %tmp = OpISub %uint %intval %test
     %zerou = OpISub %uint %tmp %int_7
     %zeroi = OpBitcast %int %zerou

)EOSHADER";

    // generate dynamic versions of the constants
    for(float f : float_constants)
    {
      std::string name = fmt::format("{}", f);
      for(char &c : name)
        if(c == '.')
          c = '_';
      ret += fmt::format("%float_dyn_{0} = OpFAdd %float %zerof %float_{0}\n", name);
      ret += fmt::format("%float_dyn_neg{0} = OpFAdd %float %zerof %float_neg{0}\n", name);
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

  void Prepare(int argc, char **argv)
  {
    optDevExts.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    vk_version = 0x10;

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
      vk_version = 0x11;

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
      vk_version = 0x12;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    make_asm_tests();

    size_t lastTest = pixel_glsl.rfind("case ");
    lastTest += sizeof("case ") - 1;

    const uint32_t numGLSLTests = atoi(pixel_glsl.c_str() + lastTest) + 1;

    const uint32_t numASMTests = (uint32_t)asm_tests.size();

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {7, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {8, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {21, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setlayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(Vec4i))}));

    // calculate number of tests, wrapping each row at 256
    uint32_t texWidth = AlignUp(std::max(numGLSLTests, numASMTests), 256U);
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
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel_glsl, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline glslpipe = createGraphicsPipeline(pipeCreateInfo);

    SPIRVTarget target = SPIRVTarget::vulkan;

    if(vk_version >= 0x11)
      target = SPIRVTarget::vulkan11;
    if(vk_version >= 0x12)
      target = SPIRVTarget::vulkan12;

    pipeCreateInfo.stages[1] = CompileShaderModule(make_pixel_asm(), ShaderLang::spvasm,
                                                   ShaderStage::frag, "main", {}, target);

    VkPipeline asmpipe = createGraphicsPipeline(pipeCreateInfo);

    float triWidth = 8.0f / float(texWidth);
    float triHeight = 8.0f / float(texHeight);

    ConstsA2V triangle[] = {
        {Vec4f(-1.0f, -1.0f, triWidth, triHeight), 0.0f, 1.0f, -1.0f},
        {Vec4f(-1.0f + triWidth, -1.0f, triWidth, triHeight), 0.0f, 1.0f, -1.0f},
        {Vec4f(-1.0f, -1.0f + triHeight, triWidth, triHeight), 0.0f, 1.0f, -1.0f},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(triangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(triangle);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    AllocatedImage queryTest(this, vkh::ImageCreateInfo(183, 347, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                                        VK_IMAGE_USAGE_SAMPLED_BIT, 4, 3),
                             VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView queryTestView = createImageView(vkh::ImageViewCreateInfo(
        queryTest.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedImage queryTestMS(
        this, vkh::ImageCreateInfo(183, 347, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT, 1, 5, VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView queryTestMSView = createImageView(vkh::ImageViewCreateInfo(
        queryTestMS.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedImage smiley(
        this, vkh::ImageCreateInfo(rgba8.width, rgba8.height, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView smileyview = createImageView(
        vkh::ImageViewCreateInfo(smiley.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM));
    AllocatedBuffer uploadBuf(this, vkh::BufferCreateInfo(rgba8.data.size() * sizeof(uint32_t),
                                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                              VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    uploadBuf.upload(rgba8.data.data(), rgba8.data.size() * sizeof(uint32_t));

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, smiley.image),
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

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, smiley.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});

      vkDeviceWaitIdle(device);
    }

    VkSampler pointsampler = VK_NULL_HANDLE;
    VkSampler linearsampler = VK_NULL_HANDLE;
    VkSampler mipsampler = VK_NULL_HANDLE;

    VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampInfo.magFilter = VK_FILTER_NEAREST;
    sampInfo.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(device, &sampInfo, NULL, &pointsampler);

    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(device, &sampInfo, NULL, &linearsampler);

    sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(device, &sampInfo, NULL, &mipsampler);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    Vec4f cbufferdata[16] = {};

    AllocatedBuffer cb(
        this, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cbufferdata[0] = Vec4f(1.1f, 2.2f, 3.3f, 4.4f);
    cbufferdata[2] = Vec4f(5.5f, 6.6f, 7.7f, 8.8f);
    cbufferdata[4] = Vec4f(9.9f, 9.99f, 9.999f, 9.999f);
    cbufferdata[6] = Vec4f(100.0f, 200.0f, 300.0f, 400.0f);

    cb.upload(cbufferdata);

    AllocatedBuffer texbuffer(
        this, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    texbuffer.upload(cbufferdata);

    AllocatedBuffer store_buffer(
        this, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedBuffer store_texbuffer(
        this, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedImage store_image(
        this, vkh::ImageCreateInfo(128, 128, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    VkImageView store_view = createImageView(vkh::ImageViewCreateInfo(
        store_image.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

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
    setName(store_texbuffer.buffer, "store_texbuffer");
    setName(store_image.image, "store_image");

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    {vkh::DescriptorBufferInfo(cb.buffer)}),
            vkh::WriteDescriptorSet(
                descset, 1, VK_DESCRIPTOR_TYPE_SAMPLER,
                {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, pointsampler)}),
            vkh::WriteDescriptorSet(
                descset, 2, VK_DESCRIPTOR_TYPE_SAMPLER,
                {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, linearsampler)}),
            vkh::WriteDescriptorSet(
                descset, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                {vkh::DescriptorImageInfo(smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          VK_NULL_HANDLE)}),
            vkh::WriteDescriptorSet(
                descset, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          linearsampler)}),
            vkh::WriteDescriptorSet(descset, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(store_buffer.buffer)}),
            vkh::WriteDescriptorSet(
                descset, 6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                {vkh::DescriptorImageInfo(store_view, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
            vkh::WriteDescriptorSet(descset, 7, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, {bufview}),
            vkh::WriteDescriptorSet(descset, 8, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                    {store_bufview}),

            vkh::WriteDescriptorSet(
                descset, 20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(queryTestView, VK_IMAGE_LAYOUT_GENERAL, mipsampler)}),
            vkh::WriteDescriptorSet(
                descset, 21, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(queryTestMSView, VK_IMAGE_LAYOUT_GENERAL, mipsampler)}),
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

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, store_image.image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, store_buffer.buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, store_texbuffer.buffer),
          });

      vkCmdClearColorImage(cmd, store_image.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(6.66f, 6.66f, 6.66f, 6.66f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdFillBuffer(cmd, store_buffer.buffer, 0, VK_WHOLE_SIZE, 0xcccccccc);
      vkCmdFillBuffer(cmd, store_texbuffer.buffer, 0, VK_WHOLE_SIZE, 0);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(
                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, store_image.image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       store_buffer.buffer),
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

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe);
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &s);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      Vec4i push = Vec4i(101, 103, 107, 109);

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(Vec4i), &push);

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "GLSL tests");
      uint32_t numTests = numGLSLTests;
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
      vkCmdSetViewport(cmd, 0, 1, &v);

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(Vec4i), &push);

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
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

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDestroySampler(device, pointsampler, NULL);
    vkDestroySampler(device, linearsampler, NULL);
    vkDestroySampler(device, mipsampler, NULL);

    return 0;
  }
};

REGISTER_TEST();
