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

#include "d3d11_test.h"

RD_TEST(D3D11_Shader_Debug_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests shader debugging in different edge cases";

  struct ConstsA2V
  {
    Vec3f pos;
    float zero;
    float one;
    float negone;
  };

  std::string common = R"EOSHADER(

struct consts
{
  float3 pos : POSITION;
  float zeroVal : ZERO;
  float oneVal : ONE;
  float negoneVal : NEGONE;
};

struct v2f
{
  float4 pos : SV_POSITION;
  float2 zeroVal : ZERO;
  float tinyVal : TINY;
  float oneVal : ONE;
  float negoneVal : NEGONE;
  uint tri : TRIANGLE;
  uint intval : INTVAL;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

v2f main(consts IN, uint tri : SV_InstanceID)
{
  v2f OUT = (v2f)0;

  OUT.pos = float4(IN.pos.x + IN.pos.z * float(tri), IN.pos.y, 0.0f, 1);

  OUT.zeroVal = IN.zeroVal.xx;
  OUT.oneVal = IN.oneVal;
  OUT.negoneVal = IN.negoneVal;
  OUT.tri = tri;
  OUT.tinyVal = IN.oneVal * 1.0e-30f;
  OUT.intval = tri + 7;

  return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

// error X3556: integer divides may be much slower, try using uints if possible.
// we want to do this on purpose
#pragma warning( disable : 3556 )

struct InnerStruct
{
  float a;
  float b[2];
  float c;
};

struct MyStruct
{
  float a;
  float4 b;
  float c;
  InnerStruct d;
  float e;
};

Buffer<float> test : register(t0);
ByteAddressBuffer byterotest : register(t1);
StructuredBuffer<MyStruct> structrotest : register(t2);
Texture2D<float> dimtex : register(t3);
Texture2DMS<float> dimtexms : register(t4);
Texture2D<float4> smiley : register(t5);
Texture2D<int4> smileyint : register(t6);
Texture2D<uint4> smileyuint : register(t7);

RWByteAddressBuffer byterwtest : register(u1);
RWStructuredBuffer<MyStruct> structrwtest : register(u2);
RWBuffer<float4> typedrwtest : register(u3);

Buffer<float> unboundsrv1 : register(t100);
Texture2D<float> unboundsrv2 : register(t101);

Buffer<float4> rgb_srv : register(t102);

RWBuffer<float> unbounduav1 : register(u4);
RWTexture2D<float> unbounduav2 : register(u5);

SamplerState linearclamp : register(s0);
SamplerState linearwrap : register(s1);
SamplerState unboundsamp : register(s2);

float4 main(v2f IN) : SV_Target0
{
  float  posinf = IN.oneVal/IN.zeroVal.x;
  float  neginf = IN.negoneVal/IN.zeroVal.x;
  float  nan = IN.zeroVal.x/IN.zeroVal.y;

  float negone = IN.negoneVal;
  float posone = IN.oneVal;
  float zero = IN.zeroVal.x;
  float tiny = IN.tinyVal;

  int intval = IN.intval;

  if(IN.tri == 0)
    return float4(log(negone), log(zero), log(posone), 1.0f);
  if(IN.tri == 1)
    return float4(log(posinf), log(neginf), log(nan), 1.0f);
  if(IN.tri == 2)
    return float4(exp(negone), exp(zero), exp(posone), 1.0f);
  if(IN.tri == 3)
    return float4(exp(posinf), exp(neginf), exp(nan), 1.0f);
  if(IN.tri == 4)
    return float4(sqrt(negone), sqrt(zero), sqrt(posone), 1.0f);
  if(IN.tri == 5)
    return float4(sqrt(posinf), sqrt(neginf), sqrt(nan), 1.0f);
  if(IN.tri == 6)
    return float4(rsqrt(negone), rsqrt(zero), rsqrt(posone), 1.0f);
  if(IN.tri == 7)
    return float4(saturate(posinf), saturate(neginf), saturate(nan), 1.0f);
  if(IN.tri == 8)
    return float4(min(posinf, nan), min(neginf, nan), min(nan, nan), 1.0f);
  if(IN.tri == 9)
    return float4(min(posinf, posinf), min(neginf, posinf), min(nan, posinf), 1.0f);
  if(IN.tri == 10)
    return float4(min(posinf, neginf), min(neginf, neginf), min(nan, neginf), 1.0f);
  if(IN.tri == 11)
    return float4(max(posinf, nan), max(neginf, nan), max(nan, nan), 1.0f);
  if(IN.tri == 12)
    return float4(max(posinf, posinf), max(neginf, posinf), max(nan, posinf), 1.0f);
  if(IN.tri == 13)
    return float4(max(posinf, neginf), max(neginf, neginf), max(nan, neginf), 1.0f);

  // rounding tests
  float round_a = 1.7f*posone;
  float round_b = 2.1f*posone;
  float round_c = 1.5f*posone;
  float round_d = 2.5f*posone;
  float round_e = zero;
  float round_f = -1.7f*posone;
  float round_g = -2.1f*posone;
  float round_h = -1.5f*posone;
  float round_i = -2.5f*posone;

  if(IN.tri == 14)
    return float4(round(round_a), floor(round_a), ceil(round_a), trunc(round_a));
  if(IN.tri == 15)
    return float4(round(round_b), floor(round_b), ceil(round_b), trunc(round_b));
  if(IN.tri == 16)
    return float4(round(round_c), floor(round_c), ceil(round_c), trunc(round_c));
  if(IN.tri == 17)
    return float4(round(round_d), floor(round_d), ceil(round_d), trunc(round_d));
  if(IN.tri == 18)
    return float4(round(round_e), floor(round_e), ceil(round_e), trunc(round_e));
  if(IN.tri == 19)
    return float4(round(round_f), floor(round_f), ceil(round_f), trunc(round_f));
  if(IN.tri == 20)
    return float4(round(round_g), floor(round_g), ceil(round_g), trunc(round_g));
  if(IN.tri == 21)
    return float4(round(round_h), floor(round_h), ceil(round_h), trunc(round_h));
  if(IN.tri == 22)
    return float4(round(round_i), floor(round_i), ceil(round_i), trunc(round_i));

  if(IN.tri == 23)
    return float4(round(neginf), floor(neginf), ceil(neginf), trunc(neginf));
  if(IN.tri == 24)
    return float4(round(posinf), floor(posinf), ceil(posinf), trunc(posinf));
  if(IN.tri == 25)
    return float4(round(nan), floor(nan), ceil(nan), trunc(nan));

  if(IN.tri == 26)
    return test[5].xxxx;

  if(IN.tri == 27)
  {
    uint unsignedVal = uint(344.1f*posone);
    int signedVal = int(344.1f*posone);
    return float4(firstbithigh(unsignedVal), firstbitlow(unsignedVal),
                  firstbithigh(signedVal), firstbitlow(signedVal));
  }

  if(IN.tri == 28)
  {
    int signedVal = int(344.1f*negone);
    return float4(firstbithigh(signedVal), firstbitlow(signedVal), 0.0f, 0.0f);
  }

  // saturate NaN returns 0
  if(IN.tri == 29)
    return float4(0.1f+saturate(nan * 2.0f), 0.1f+saturate(nan * 3.0f), 0.1f+saturate(nan * 4.0f), 1.0f);

  // min() and max() with NaN return the other component if it's non-NaN, or else nan if it is nan
  if(IN.tri == 30)
    return float4(min(nan, 0.3f), max(nan, 0.3f), max(nan, nan), 1.0f);

  // the above applies componentwise
  if(IN.tri == 31)
    return max( float4(0.1f, 0.2f, 0.3f, 0.4f), nan.xxxx );
  if(IN.tri == 32)
    return min( float4(0.1f, 0.2f, 0.3f, 0.4f), nan.xxxx );

  // negating nan and abs(nan) gives nan
  if(IN.tri == 33)
    return float4(-nan, abs(nan), 0.0f, 1.0f);

  // check denorm flushing
  if(IN.tri == 34)
    return float4(tiny * 1.5e-8f, tiny * 1.5e-9f, asfloat(intval) == 0.0f ? 1.0f : 0.0f, 1.0f);

  // test reading/writing byte address data

  // mis-aligned loads
  if(IN.tri == 35) // undefined-test
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    return float4(asfloat(byterotest.Load(z+0).x), asfloat(byterotest.Load(z+1).x),
                  asfloat(byterotest.Load(z+3).x), float(byterotest.Load(z+8).x));
  }
  // later loads: valid, out of view bounds but in buffer bounds, out of both bounds
  if(IN.tri == 36)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    return float4(asfloat(byterotest.Load(z+40).x), asfloat(byterotest.Load(z+44).x),
                  asfloat(byterotest.Load(z+48).x), float(byterotest.Load(z+4096).x));
  }
  // 4-uint load
  if(IN.tri == 37)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    // test a 4-uint load
    return asfloat(byterotest.Load4(z+24));
  }
  // 4-uint load crossing view bounds
  if(IN.tri == 38)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    // test a 4-uint load
    return asfloat(byterotest.Load4(z+40));
  }
  // 4-uint load out of view bounds
  if(IN.tri == 39)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    // test a 4-uint load
    return asfloat(byterotest.Load4(z+48));
  }

  // mis-aligned store
  if(IN.tri == 40) // undefined-test
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store(z+0, asuint(5.4321f));
    byterwtest.Store(z+1, asuint(9.8765f));

    return asfloat(byterwtest.Load(z2+0).x);
  }
  // mis-aligned loads
  if(IN.tri == 41) // undefined-test
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store(z+0, asuint(5.4321f));
    byterwtest.Store(z+4, asuint(9.8765f));
    byterwtest.Store(z+8, 0xbeef);

    return float4(asfloat(byterwtest.Load(z2+0).x), asfloat(byterwtest.Load(z2+1).x),
                  asfloat(byterwtest.Load(z2+3).x), float(byterwtest.Load(z2+8).x));
  }
  // later stores: valid, out of view bounds but in buffer bounds, out of both bounds
  if(IN.tri == 42)
  {
    // use this to ensure the compiler doesn't know we're loading from the same locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store(z+40, asuint(1.2345f));
    byterwtest.Store(z+44, asuint(9.8765f));
    byterwtest.Store(z+48, asuint(1.81818f));
    byterwtest.Store(z+4096, asuint(5.55555f));

    return float4(asfloat(byterwtest.Load(z2+40).x), asfloat(byterwtest.Load(z2+44).x),
                  asfloat(byterwtest.Load(z2+48).x), float(byterwtest.Load(z2+4096).x));
  }
  // 4-uint store
  if(IN.tri == 43)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store4(z+24, uint4(99, 88, 77, 66));

    return asfloat(byterotest.Load4(z2+24));
  }
  // 4-uint store crossing view bounds
  if(IN.tri == 44)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store4(z+40, uint4(99, 88, 77, 66));

    return asfloat(byterotest.Load4(z2+40));
  }
  // 4-uint store out of view bounds
  if(IN.tri == 45)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store4(z+48, uint4(99, 88, 77, 66));

    return asfloat(byterotest.Load4(z2+48));
  }

  // test reading/writing structured data

  // reading struct at 0 (need two tests to verify most of the data,
  // we assume the rest is OK because of alignment)
  if(IN.tri == 46)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+0];

    return float4(read.b.xyz, read.c);
  }
  if(IN.tri == 47)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+0];

    return float4(read.a, read.e, read.d.b[z+0], read.d.c);
  }
  // reading later, but in bounds
  if(IN.tri == 48)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+3];

    return float4(read.b.xyz, read.c);
  }
  if(IN.tri == 49)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+3];

    return float4(read.a, read.e, read.d.b[z+0], read.d.c);
  }
  // structured buffers do not allow partially out of bounds behaviour:
  // - buffers must by multiples of structure stride (so buffer partials aren't allowed)
  // - views work in units of structure stride (so view partials aren't allowed)
  // we can only test fully out of bounds of the view, but in bounds of the buffer
  if(IN.tri == 50)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+7];

    return float4(read.b.xyz, read.c);
  }
)EOSHADER"
                      R"EOSHADER(
  if(IN.tri == 51)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+7];

    return float4(read.a, read.e, read.d.b[z+0], read.d.c);
  }

  // storing in bounds
  if(IN.tri == 52)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    MyStruct write = (MyStruct)0;

    write.a = zero+1.0f;
    write.c = zero+2.0f;
    write.e = zero+3.0f;
    write.b = float4(zero+4.0f, zero+5.0f, zero+6.0f, zero+7.0f);
    write.d.a = zero+8.0f;
    write.d.b[0] = zero+9.0f;
    write.d.b[1] = zero+10.0f;
    write.d.c = zero+11.0f;

    structrwtest[z+2] = write;

    MyStruct read = structrwtest[z2+2];

    return float4(read.b.xyz, read.c);
  }
  if(IN.tri == 53)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    MyStruct write = (MyStruct)0;

    write.a = zero+1.0f;
    write.c = zero+2.0f;
    write.e = zero+3.0f;
    write.b = float4(zero+4.0f, zero+5.0f, zero+6.0f, zero+7.0f);
    write.d.a = zero+8.0f;
    write.d.b[0] = zero+9.0f;
    write.d.b[1] = zero+10.0f;
    write.d.c = zero+11.0f;

    structrwtest[z+2] = write;

    MyStruct read = structrwtest[z2+2];

    return float4(read.a, read.e, read.d.b[z2+0], read.d.c);
  }

  // storing out of bounds
  if(IN.tri == 54)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    MyStruct write = (MyStruct)0;

    write.a = zero+1.0f;
    write.c = zero+2.0f;
    write.e = zero+3.0f;
    write.b = float4(zero+4.0f, zero+5.0f, zero+6.0f, zero+7.0f);
    write.d.a = zero+8.0f;
    write.d.b[0] = zero+9.0f;
    write.d.b[1] = zero+10.0f;
    write.d.c = zero+11.0f;

    structrwtest[z+7] = write;

    MyStruct read = structrwtest[z2+7];

    return float4(read.b.xyz, read.c);
  }
  if(IN.tri == 55)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    MyStruct write = (MyStruct)0;

    write.a = zero+1.0f;
    write.c = zero+2.0f;
    write.e = zero+3.0f;
    write.b = float4(zero+4.0f, zero+5.0f, zero+6.0f, zero+7.0f);
    write.d.a = zero+8.0f;
    write.d.b[0] = zero+9.0f;
    write.d.b[1] = zero+10.0f;
    write.d.c = zero+11.0f;

    structrwtest[z+7] = write;

    MyStruct read = structrwtest[z2+7];

    return float4(read.a, read.e, read.d.b[z2+0], read.d.c);
  }
  if(IN.tri == 56)
  {
    uint width = 0, height = 0, numLevels = 0;
    dimtex.GetDimensions(0, width, height, numLevels);
    return float4(width, height, numLevels, 0.0f);
  }
  if(IN.tri == 57)
  {
    uint width = 0, height = 0, numLevels = 0;
    dimtex.GetDimensions(2, width, height, numLevels);
    return float4(width, height, numLevels, 0.0f);
  }
  if(IN.tri == 58)
  {
    uint width = 0, height = 0, numLevels = 0;
    dimtex.GetDimensions(10, width, height, numLevels);
    return float4(max(1,width), max(1,height), numLevels, 0.0f);
  }

  if(IN.tri == 59)
  {
    // use this to ensure the compiler doesn't know we're using fixed mips
    uint z = intval - IN.tri - 7;

    uint width = 0, height = 0, numLevels = 0;
    dimtex.GetDimensions(z, width, height, numLevels);
    return float4(width, height, numLevels, 0.0f);
  }
  if(IN.tri == 60)
  {
    // use this to ensure the compiler doesn't know we're using fixed mips
    uint z = intval - IN.tri - 7;

    uint width = 0, height = 0, numLevels = 0;
    dimtex.GetDimensions(z+2, width, height, numLevels);
    return float4(width, height, numLevels, 0.0f);
  }
  if(IN.tri == 61)
  {
    // use this to ensure the compiler doesn't know we're using fixed mips
    uint z = intval - IN.tri - 7;

    uint width = 0, height = 0, numLevels = 0;
    dimtex.GetDimensions(z+10, width, height, numLevels);
    return float4(max(1,width), max(1,height), numLevels, 0.0f);
  }
  if(IN.tri == 62)
  {
    uint width = 0;
    test.GetDimensions(width);
    return float4(max(1,width), 0.0f, 0.0f, 0.0f);
  }
  if(IN.tri == 63)
  {
    uint width = 0, height = 0, numSamples = 0;
    dimtexms.GetDimensions(width, height, numSamples);
    return float4(width, height, numSamples, 0.0f);
  }
  if(IN.tri == 64)
  {
    uint width = 0, height = 0, numSamples = 0;
    dimtexms.GetDimensions(width, height, numSamples);
    float2 posLast = dimtexms.GetSamplePosition(numSamples - 1);
    return float4(posLast, 0.0f, 0.0f);
  }
  if(IN.tri == 65)
  {
    uint width = 0, height = 0, numSamples = 0;
    dimtexms.GetDimensions(width, height, numSamples);
    float2 posInvalid = dimtexms.GetSamplePosition(numSamples + 1);
    return float4(posInvalid, 0.0f, 0.0f);
  }
  if(IN.tri == 66)
  {
    // Test sampleinfo with a non-MSAA rasterizer
    uint numSamples = GetRenderTargetSampleCount();
    float2 pos = GetRenderTargetSamplePosition(0);
    return float4(pos, numSamples, 0.0f);
  }
  if(IN.tri == 67)
  {
    float val = posone * 1.8631f;
    float a = 0.0f, b = 0.0f;
    sincos(val, a, b);
    return float4(val, a, b, 0.0f);
  }
  if(IN.tri == 68)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    // try to force a swizzle on the load
    return asfloat(byterotest.Load4(z+0).yz).xyxy;
  }
  if(IN.tri == 69)
  {
    float2 uv = posone * float2(1.81f, 0.48f);
    return smiley.Sample(linearclamp, uv);
  }
  if(IN.tri == 70)
  {
    float2 uv = posone * float2(1.81f, 0.48f);
    return smiley.Sample(linearwrap, uv);
  }
  if(IN.tri == 71)
  {
    float2 uv = posone * float2(1.81f, 0.48f) / zero;
    return smiley.Sample(linearclamp, uv);
  }
  if(IN.tri == 72)
  {
    return unboundsrv1[0].xxxx;
  }
  if(IN.tri == 73)
  {
    return unboundsrv2.Load(int3(0, 0, 0)).xxxx;
  }
  if(IN.tri == 74)
  {
    return unboundsrv2.Sample(linearclamp, float2(0, 0)).xxxx;
  }
  if(IN.tri == 75)
  {
    return unbounduav1[0].xxxx;
  }
  if(IN.tri == 76)
  {
    unbounduav1[1] = 1.234f;
    return unbounduav1[1].xxxx;
  }
  if(IN.tri == 77)
  {
    unbounduav2[int2(0, 1)] = 1.234f;
    return unbounduav2[int2(0, 1)].xxxx;
  }
  if(IN.tri == 78)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    // read first. This should be zero
    float read_val = asfloat(byterwtest.Load(z2+100).x);

    byterwtest.Store(z+100, asuint(1.2345f));

    return read_val;
  }
  if(IN.tri == 79)
  {
    return rgb_srv[0];
  }
  if(IN.tri == 80)
  {
    uint z2 = uint(zero);
    MyStruct read = structrwtest[z2+7];

    return read.b.xyzw;
  }
  if(IN.tri == 81)
  {
    uint z2 = uint(zero);
    MyStruct read = structrwtest[z2+7];

    return read.b.zzyx;
  }
  if(IN.tri == 82)
  {
    uint z2 = uint(zero);
    MyStruct read = structrwtest[z2+7];

    return read.b.zwxy;
  }
  if(IN.tri == 83)
  {
    uint z2 = uint(zero);
    MyStruct read = structrwtest[z2+7];

    return read.b.wzwy;
  }
#ifdef TYPED_UAV_EXT
  if(IN.tri == 84)
  {
    return typedrwtest[uint(zero)].xyzw;
  }
  if(IN.tri == 85)
  {
    return typedrwtest[uint(zero)].zzyx;
  }
  if(IN.tri == 86)
  {
    return typedrwtest[uint(zero)].zwxy;
  }
  if(IN.tri == 87)
  {
    return typedrwtest[uint(zero)].wzwy;
  }
#endif
  if(IN.tri == 88)
  {
    float2 uv = posone * float2(0.55f, 0.48f);
    return smiley.Sample(linearwrap, uv, int2(4, 3));
  }
  if(IN.tri == 89)
  {
    float2 uv = posone * float2(1.81f, 0.48f);
    return smileyint.Load(int3(uv*16,0));
  }
  if(IN.tri == 90)
  {
    float2 uv = posone * float2(1.81f, 0.48f);
    return smileyuint.Load(int3(uv*16,0));
  }
  if(IN.tri == 91)
  {
    float2 uv = posone * float2(0.55f, 0.48f);
    return smiley.Sample(unboundsamp, uv);
  }
  if(IN.tri == 92)
  {
    float2 uv = posone * float2(0.55f, 0.48f);
    return smiley.SampleBias(unboundsamp, uv, 0.5f);
  }

  return float4(0.4f, 0.4f, 0.4f, 0.4f);
}

)EOSHADER";

  std::string flowPixel = R"EOSHADER(

float4 main(v2f IN) : SV_Target0 
{
  uint zero = IN.tri;

  float4 ret = float4(0,0,0,0);

  // test multiple ifs
  if(zero < 5)
  {
    ret.w += 2.0f;
  }
  else
  {
    ret.w += 4.0f;
  }

  if(zero > 1)
  {
    ret.w += 8.0f;
  }
  else
  {
    ret.w += 16.0f;
  }

  // test nested ifs
  if(zero < 5)
  {
    if(zero > 1)
    {
      ret.z += 2.0f;
    }
    else
    {
      ret.z += 4.0f;
    }
  }
  else
  {
    if(zero < 10)
    {
      ret.z += 8.0f;
    }
    else
    {
      ret.z += 16.0f;
    }
  }

  // test loops
  ret.y = 1.0f;
  for(uint i=0; i < zero + 5; i++)
  {
    ret.y += 1.0f;
  }

  for(uint j=0; j < zero; j++)
  {
    ret.y += 100.0f;
  }

  for(uint k=0; k < zero + 2; k++)
  {
    for(uint l=0; l < zero + 3; l++)
    {
      ret.y += 10.0f;
    }
  }

  // test switches
  switch(zero)
  {
    // fallthrough
    case 1:
    case 0:
      ret.x += 1.0f;
      break;
    case 3:
    case 4:
      ret.x += 2.0f;
      break;
    default:
      break;
  }

  switch(zero+4)
  {
    // fallthrough
    case 1:
    case 0:
      ret.x += 4.0f;
      break;
    case 3:
    case 4:
      ret.x += 8.0f;
      break;
    default:
      break;
  }

  return ret;
}

)EOSHADER";

  std::string msaaPixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Buffer<float> test : register(t0);

Texture2D<float4> tex : register(t3);
SamplerState linearclamp : register(s0);

float4 main(v2f IN, uint samp : SV_SampleIndex) : SV_Target0 
{
  float2 uvCentroid = EvaluateAttributeCentroid(IN.uv);
  float2 uvSamp0 = EvaluateAttributeAtSample(IN.uv, 0) - IN.uv;
  float2 uvSampThis = EvaluateAttributeAtSample(IN.uv, samp) - IN.uv;
  float2 uvOffset = EvaluateAttributeSnapped(IN.uv, int2(1, 1));

  float x = (uvCentroid.x + uvCentroid.y) * 0.5f;
  float y = (uvSamp0.x + uvSamp0.y) * 0.5f;
  float z = (uvSampThis.x + uvSampThis.y) * 0.5f;
  float w = (uvOffset.x + uvOffset.y) * 0.5f;

  // Test sampleinfo with a MSAA rasterizer
  uint numSamples = 100;
  float2 pos = float2(99.9f, 99.9f);

  uint width = 3;

  // do a condition that relies on texture samples and math operations so that we can check that
  // evaluating those has no side-effects
  if(IN.pos.x + sin(IN.pos.y) + tex.Sample(linearclamp, IN.uv).z < 1000.0f)
  {
    // RT should still have the same properties
    numSamples = GetRenderTargetSampleCount();
    pos = GetRenderTargetSamplePosition(samp);

    // SRV bound at slot 0 should still be the buffer
    test.GetDimensions(width);
  }

  return float4(x + pos.x, y + pos.y, z + (float)numSamples + (float)width, w);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    size_t lastTest = pixel.rfind("IN.tri == ");
    lastTest += sizeof("IN.tri == ") - 1;

    const uint32_t numTests = atoi(pixel.c_str() + lastTest) + 1;

    std::string undefined_tests = "Undefined tests:";

    size_t undef = pixel.find("undefined-test");
    while(undef != std::string::npos)
    {
      size_t testNumStart = pixel.rfind("IN.tri == ", undef);
      testNumStart += sizeof("IN.tri == ") - 1;
      size_t testNumEnd = pixel.find_first_not_of("0123456789", testNumStart);

      undefined_tests += " ";
      undefined_tests += pixel.substr(testNumStart, testNumEnd - testNumStart);

      undef = pixel.find("undefined-test", undef + 1);
    }

    if(opts2.TypedUAVLoadAdditionalFormats)
      common += "\n#define TYPED_UAV_EXT 1\n";

    ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(common + pixel, "main", "ps_5_0");

    D3D11_INPUT_ELEMENT_DESC layoutdesc[] = {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0,
        },
        {
            "ZERO",
            0,
            DXGI_FORMAT_R32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0,
        },
        {
            "ONE",
            0,
            DXGI_FORMAT_R32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0,
        },
        {
            "NEGONE",
            0,
            DXGI_FORMAT_R32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0,
        },
    };

    ID3D11InputLayoutPtr layout;
    CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                    vsblob->GetBufferSize(), &layout));

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);
    ID3D11PixelShaderPtr flowps = CreatePS(Compile(common + flowPixel, "main", "ps_5_0"));

    static const uint32_t texDim = AlignUp(numTests, 64U) * 4;

    ID3D11Texture2DPtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, texDim, 8).RTV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    float triWidth = 8.0f / float(texDim);

    ConstsA2V triangle[] = {
        {Vec3f(-1.0f, -1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f + triWidth, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(triangle);

    union
    {
      float f;
      uint32_t u;
    } pun;

    pun.u = 0xdead;

    float testdata[] = {
        1.0f,  2.0f,  3.0f,  4.0f,  1.234567f, pun.f, 7.0f,  8.0f,  9.0f,  10.0f,
        11.0f, 12.0f, 13.0f, 14.0f, 15.0f,     16.0f, 17.0f, 18.0f, 19.0f, 20.0f,
    };

    ID3D11BufferPtr srvBuf = MakeBuffer().SRV().Data(testdata);
    ID3D11ShaderResourceViewPtr srv = MakeSRV(srvBuf).Format(DXGI_FORMAT_R32_FLOAT);

    ID3D11Texture2DPtr testTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16).Mips(3).SRV();
    ID3D11ShaderResourceViewPtr testSRV = MakeSRV(testTex);

    ID3D11Texture2DPtr msTex = MakeTexture(DXGI_FORMAT_R32_FLOAT, 16, 16).Multisampled(4).RTV().SRV();
    ID3D11ShaderResourceViewPtr msSRV = MakeSRV(msTex);

    ID3D11BufferPtr rawBuf = MakeBuffer().SRV().ByteAddressed().Data(testdata);
    ID3D11ShaderResourceViewPtr rawsrv =
        MakeSRV(rawBuf).Format(DXGI_FORMAT_R32_TYPELESS).FirstElement(4).NumElements(12);

    ID3D11BufferPtr rawBuf2 = MakeBuffer().UAV().ByteAddressed().Size(1024);
    ID3D11UnorderedAccessViewPtr rawuav =
        MakeUAV(rawBuf2).Format(DXGI_FORMAT_R32_TYPELESS).FirstElement(4).NumElements(12);

    float structdata[220];
    for(int i = 0; i < 220; i++)
      structdata[i] = float(i);

    ID3D11BufferPtr rgbBuf = MakeBuffer().SRV().Data(structdata);
    ID3D11ShaderResourceViewPtr rgbsrv = MakeSRV(rgbBuf).Format(DXGI_FORMAT_R32G32B32_FLOAT);

    ID3D11BufferPtr structBuf = MakeBuffer().SRV().Structured(11 * sizeof(float)).Data(structdata);
    ID3D11ShaderResourceViewPtr structsrv =
        MakeSRV(structBuf).Format(DXGI_FORMAT_UNKNOWN).FirstElement(3).NumElements(5);

    ID3D11BufferPtr structBuf2 = MakeBuffer().UAV().Structured(11 * sizeof(float)).Size(880);
    ID3D11UnorderedAccessViewPtr structuav =
        MakeUAV(structBuf2).Format(DXGI_FORMAT_UNKNOWN).FirstElement(3).NumElements(5);

    ID3D11BufferPtr rgbuavBuf = MakeBuffer().UAV().Data(structdata);
    ID3D11UnorderedAccessViewPtr typeuav = MakeUAV(rgbuavBuf).Format(DXGI_FORMAT_R32G32B32A32_FLOAT);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    ID3D11Texture2DPtr smiley =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_TYPELESS, rgba8.width, rgba8.height).SRV();
    ID3D11ShaderResourceViewPtr smileysrv = MakeSRV(smiley).Format(DXGI_FORMAT_R8G8B8A8_UNORM);
    ID3D11ShaderResourceViewPtr smileyintsrv = MakeSRV(smiley).Format(DXGI_FORMAT_R8G8B8A8_SINT);
    ID3D11ShaderResourceViewPtr smileyuintsrv = MakeSRV(smiley).Format(DXGI_FORMAT_R8G8B8A8_UINT);

    ctx->UpdateSubresource(smiley, 0, NULL, rgba8.data.data(), rgba8.width * sizeof(uint32_t), 0);

    ID3D11ShaderResourceView *srvs[] = {
        srv, rawsrv, structsrv, testSRV, msSRV, smileysrv, smileyintsrv, smileyuintsrv,
    };

    ctx->PSSetShaderResources(0, ARRAY_COUNT(srvs), srvs);

    ctx->PSSetShaderResources(102, 1, &rgbsrv.GetInterfacePtr());

    // Create resources for MSAA draw
    ID3DBlobPtr vsmsaablob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psmsaablob = Compile(msaaPixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsmsaablob);

    ID3D11SamplerStatePtr linearclamp = MakeSampler();
    ctx->PSSetSamplers(0, 1, &linearclamp.GetInterfacePtr());
    ID3D11SamplerStatePtr linearwrap = MakeSampler();
    ctx->PSSetSamplers(1, 1, &linearwrap.GetInterfacePtr());

    ID3D11VertexShaderPtr vsmsaa = CreateVS(vsmsaablob);
    ID3D11PixelShaderPtr psmsaa = CreatePS(psmsaablob);

    ID3D11BufferPtr vbmsaa = MakeBuffer().Vertex().Data(DefaultTri);

    ID3D11Texture2DPtr msaaTex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 8, 8).Multisampled(4).RTV();
    ID3D11RenderTargetViewPtr msaaRT = MakeRTV(msaaTex);

    while(Running())
    {
      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(ConstsA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(layout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)texDim, 4.0f, 0.0f, 1.0f});

      UINT zero[4] = {};
      ctx->ClearUnorderedAccessViewUint(rawuav, zero);
      ctx->ClearUnorderedAccessViewUint(structuav, zero);
      ID3D11UnorderedAccessView *uavs[] = {rawuav, structuav, typeuav};
      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &fltRT.GetInterfacePtr(), NULL, 1, 3, uavs,
                                                     NULL);

      setMarker(undefined_tests);

      setMarker("Main Test");
      ctx->DrawInstanced(3, numTests, 0, 0);

      RSSetViewport({0.0f, 4.0f, (float)texDim, 4.0f, 0.0f, 1.0f});
      ctx->PSSetShader(flowps, NULL, 0);
      setMarker("Flow Test");
      ctx->DrawInstanced(3, 1, 0, 0);

      ctx->OMSetRenderTargets(1, &msaaRT.GetInterfacePtr(), NULL);

      RSSetViewport({0.0f, 0.0f, 8.0f, 8.0f, 0.0f, 1.0f});
      IASetVertexBuffer(vbmsaa, sizeof(DefaultA2V), 0);
      ctx->IASetInputLayout(defaultLayout);
      ctx->VSSetShader(vsmsaa, NULL, 0);
      ctx->PSSetShader(psmsaa, NULL, 0);
      setMarker("MSAA Test");
      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
