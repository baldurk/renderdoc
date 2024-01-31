/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "d3d12_test.h"

RD_TEST(D3D12_Shader_Debug_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests shader debugging in different edge cases";

  struct ConstsA2V
  {
    Vec3f pos;
    float zero;
    float one;
    float negone;
  };

  std::string vertexSampleVS = R"EOSHADER(

Texture2D<float4> intex : register(t0);

struct v2f { float4 pos : SV_Position; float4 col : COL; };

v2f main(uint vid : SV_VertexID)
{
	float2 positions[] = {
		float2(-1.0f,  1.0f),
		float2( 1.0f,  1.0f),
		float2(-1.0f, -1.0f),
		float2( 1.0f, -1.0f),
	};

  v2f ret = (v2f)0;
	ret.pos = float4(positions[vid], 0, 1);
  ret.col = intex.Load(float3(0,0,0));
  return ret;
}

)EOSHADER";

  std::string vertexSamplePS = R"EOSHADER(

struct v2f { float4 pos : SV_Position; float4 col : COL; };

float4 main(v2f IN) : SV_Target0
{
	return IN.col;
}

)EOSHADER";

  std::string pixelBlit = R"EOSHADER(

cbuffer rootconsts : register(b0)
{
  float offset;
}

Texture2D<float4> intex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target0
{
	return intex.Load(float3(pos.x, pos.y - offset, 0));
}

)EOSHADER";

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

Buffer<float> unboundsrv1 : register(t100);
Texture2D<float> unboundsrv2 : register(t101);

RWBuffer<float> unbounduav1 : register(u4);
RWTexture2D<float> unbounduav2 : register(u5);

RWBuffer<float> narrowtypeduav : register(u6);
Buffer<float> narrowtypedsrv : register(t102);

Buffer<float4> rgb_srv : register(t103);

SamplerState linearclamp : register(s0);

StructuredBuffer<MyStruct> rootsrv : register(t20);
StructuredBuffer<MyStruct> appendsrv : register(t40);
Texture2D<float> dimtex_edge : register(t41);

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
  if(IN.tri == 51)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    MyStruct read = structrotest[z+7];

    return float4(read.a, read.e, read.d.b[z+0], read.d.c);
  }
)EOSHADER"
                      R"EOSHADER(

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
    return unboundsrv1[0].xxxx;
  }
  if(IN.tri == 69)
  {
    return unboundsrv2.Load(int3(0, 0, 0)).xxxx;
  }
  if(IN.tri == 70)
  {
    return unboundsrv2.Sample(linearclamp, float2(0, 0)).xxxx;
  }
  if(IN.tri == 71)
  {
    return unbounduav1[0].xxxx;
  }
  if(IN.tri == 72)
  {
    unbounduav1[1] = 1.234f;
    return unbounduav1[1].xxxx;
  }
  if(IN.tri == 73)
  {
    unbounduav2[int2(0, 1)] = 1.234f;
    return unbounduav2[int2(0, 1)].xxxx;
  }
  if(IN.tri == 74)
  {
    return float4(narrowtypedsrv[1], narrowtypedsrv[2], narrowtypedsrv[3], narrowtypedsrv[4]);
  }
  if(IN.tri == 75)
  {
    narrowtypeduav[13] = 555.0f;
    narrowtypeduav[14] = 888.0f;
    return float4(narrowtypeduav[11], narrowtypeduav[12], narrowtypeduav[13], narrowtypeduav[14]);
  }
  if(IN.tri == 76)
  {
    return rgb_srv[0];
  }
  if(IN.tri == 77)
  {
    float2 uv = posone * float2(0.55f, 0.48f);
    return smiley.Sample(linearclamp, uv, int2(4, 3));
  }
  if(IN.tri == 78)
  {
    uint z = intval - IN.tri - 7;

    MyStruct read = rootsrv[z+0];

    return float4(read.b.xyz, read.c);
  }
  if(IN.tri == 79)
  {
    uint z = intval - IN.tri - 7;

    MyStruct read = appendsrv[z+0];

    return float4(read.b.xyz, read.c);
  }
  if(IN.tri == 80)
  {
    // use this to ensure the compiler doesn't know we're using fixed mips
    uint z = intval - IN.tri - 7;

    uint width = 0, height = 0, numLevels = 0;
    dimtex_edge.GetDimensions(z, width, height, numLevels);
    return float4(max(1,width), max(1,height), numLevels, 0.0f);
  }
  if(IN.tri == 81)
  {
    float2 uv = posone * float2(0.55f, 0.48f);
    return smileyint.Load(int3(uv*16,0));
  }

  return float4(0.4f, 0.4f, 0.4f, 0.4f);
}

)EOSHADER";

  std::string msaaPixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

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
  uint numSamples = GetRenderTargetSampleCount();
  float2 pos = GetRenderTargetSamplePosition(samp);

  return float4(x + pos.x, y + pos.y, z + (float)numSamples, w);
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
    ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(common + pixel, "main", "ps_5_0");

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    inputLayout.reserve(4);
    inputLayout.push_back({
        "POSITION",
        0,
        DXGI_FORMAT_R32G32B32_FLOAT,
        0,
        0,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0,
    });
    inputLayout.push_back({
        "ZERO",
        0,
        DXGI_FORMAT_R32_FLOAT,
        0,
        D3D12_APPEND_ALIGNED_ELEMENT,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0,
    });
    inputLayout.push_back({
        "ONE",
        0,
        DXGI_FORMAT_R32_FLOAT,
        0,
        D3D12_APPEND_ALIGNED_ELEMENT,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0,
    });
    inputLayout.push_back({
        "NEGONE",
        0,
        DXGI_FORMAT_R32_FLOAT,
        0,
        D3D12_APPEND_ALIGNED_ELEMENT,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0,
    });

    D3D12_STATIC_SAMPLER_DESC staticSamp = {};
    staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamp.AddressU = staticSamp.AddressV = staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE1 multiRanges[3] = {
        {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            2,
            30,
            0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            30,
        },
        {
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            3,
            32,
            0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        },
        {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            2,
            40,
            0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        },
    };
    D3D12_ROOT_PARAMETER1 multiRangeParam;
    multiRangeParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    multiRangeParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    multiRangeParam.DescriptorTable.NumDescriptorRanges = ARRAY_COUNT(multiRanges);
    multiRangeParam.DescriptorTable.pDescriptorRanges = multiRanges;

    ID3D12RootSignaturePtr sig = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 8, 0),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 2, 10),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 100, 5, 20),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4, 3, 30),
            multiRangeParam,
            uavParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 21),
            srvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 20),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);

    ID3D12PipelineStatePtr pso_5_0 = MakePSO()
                                         .RootSig(sig)
                                         .InputLayout(inputLayout)
                                         .VS(vsblob)
                                         .PS(psblob)
                                         .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});

    // Recompile the same PS with SM 5.1 to test shader debugging with the different bytecode
    psblob = Compile(common + "\n#define SM_5_1 1\n" + pixel, "main", "ps_5_1");
    ID3D12PipelineStatePtr pso_5_1 = MakePSO()
                                         .RootSig(sig)
                                         .InputLayout(inputLayout)
                                         .VS(vsblob)
                                         .PS(psblob)
                                         .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});

    static const uint32_t texDim = AlignUp(numTests, 64U) * 4;

    ID3D12ResourcePtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, texDim, 4)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE fltRTV = MakeRTV(fltTex).CreateCPU(0);
    D3D12_GPU_DESCRIPTOR_HANDLE fltSRV = MakeSRV(fltTex).CreateGPU(8);

    float triWidth = 8.0f / float(texDim);

    ConstsA2V triangle[] = {
        {Vec3f(-1.0f, -1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f + triWidth, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(triangle);
    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

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

    ID3D12ResourcePtr srvBuf = MakeBuffer().Data(testdata);
    MakeSRV(srvBuf).Format(DXGI_FORMAT_R32_FLOAT).CreateGPU(0);

    ID3D12ResourcePtr testTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16).Mips(3);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3;
    dev->CreateShaderResourceView(testTex, NULL, cpu);

    {
      cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 36;

      D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
      desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.Texture2DArray.ArraySize = ~0U;
      desc.Texture2DArray.MipLevels = ~0U;

      dev->CreateShaderResourceView(testTex, &desc, cpu);
    }

    ID3D12ResourcePtr rawBuf = MakeBuffer().Data(testdata);
    MakeSRV(rawBuf)
        .Format(DXGI_FORMAT_R32_TYPELESS)
        .ByteAddressed()
        .FirstElement(4)
        .NumElements(12)
        .CreateGPU(1);

    ID3D12ResourcePtr msTex = MakeTexture(DXGI_FORMAT_R32_FLOAT, 16, 16).Multisampled(4).RTV();
    MakeSRV(msTex).CreateGPU(4);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    ID3D12ResourcePtr smiley = MakeTexture(DXGI_FORMAT_R8G8B8A8_TYPELESS, 48, 48)
                                   .Mips(1)
                                   .InitialState(D3D12_RESOURCE_STATE_COPY_DEST);

    ID3D12ResourcePtr uploadBuf = MakeBuffer().Size(1024 * 1024).Upload();
    ID3D12ResourcePtr constBuf = MakeBuffer().Size(256).Upload();
    ID3D12ResourcePtr outUAV = MakeBuffer().Size(256).UAV();
    {
      byte *mapptr = NULL;
      constBuf->Map(0, NULL, (void **)&mapptr);
      uint32_t value = 6;
      memcpy(mapptr, &value, sizeof(uint32_t));
      constBuf->Unmap(0, NULL);
    }

    {
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};

      D3D12_RESOURCE_DESC desc = smiley->GetDesc();

      dev->GetCopyableFootprints(&desc, 0, 1, 0, &layout, NULL, NULL, NULL);

      byte *srcptr = (byte *)rgba8.data.data();
      byte *mapptr = NULL;
      uploadBuf->Map(0, NULL, (void **)&mapptr);

      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      {
        D3D12_TEXTURE_COPY_LOCATION dst, src;

        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = smiley;
        dst.SubresourceIndex = 0;

        byte *dstptr = mapptr + layout.Offset;

        for(UINT row = 0; row < rgba8.height; row++)
        {
          memcpy(dstptr, srcptr, rgba8.width * sizeof(uint32_t));
          srcptr += rgba8.width * sizeof(uint32_t);
          dstptr += layout.Footprint.RowPitch;
        }

        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.pResource = uploadBuf;
        src.PlacedFootprint = layout;

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

        D3D12_RESOURCE_BARRIER b = {};
        b.Transition.pResource = smiley;
        b.Transition.Subresource = 0;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                  D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        cmd->ResourceBarrier(1, &b);
      }

      cmd->Close();

      uploadBuf->Unmap(0, NULL);

      Submit({cmd});
      GPUSync();
    }

    MakeSRV(smiley).Format(DXGI_FORMAT_R8G8B8A8_UNORM).CreateGPU(5);
    MakeSRV(smiley).Format(DXGI_FORMAT_R8G8B8A8_SINT).CreateGPU(6);
    MakeSRV(smiley).Format(DXGI_FORMAT_R8G8B8A8_UINT).CreateGPU(7);

    ID3D12ResourcePtr rawBuf2 = MakeBuffer().Size(1024).UAV();
    D3D12ViewCreator uavView1 =
        MakeUAV(rawBuf2).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().FirstElement(4).NumElements(12);
    D3D12_CPU_DESCRIPTOR_HANDLE uav1cpu = uavView1.CreateClearCPU(10);
    D3D12_GPU_DESCRIPTOR_HANDLE uav1gpu = uavView1.CreateGPU(10);

    uint16_t narrowdata[32];
    for(size_t i = 0; i < ARRAY_COUNT(narrowdata); i++)
      narrowdata[i] = MakeHalf(float(i));

    ID3D12ResourcePtr narrowtypedbuf = MakeBuffer().UAV().Data(narrowdata);
    MakeSRV(narrowtypedbuf).Format(DXGI_FORMAT_R16_FLOAT).CreateGPU(22);
    MakeUAV(narrowtypedbuf).Format(DXGI_FORMAT_R16_FLOAT).CreateGPU(32);

    float structdata[220];
    for(int i = 0; i < 220; i++)
      structdata[i] = float(i);

    ID3D12ResourcePtr rgbbuf = MakeBuffer().Data(structdata);
    MakeSRV(rgbbuf).Format(DXGI_FORMAT_R32G32B32_FLOAT).CreateGPU(23);

    ID3D12ResourcePtr structBuf = MakeBuffer().Data(structdata);
    MakeSRV(structBuf)
        .Format(DXGI_FORMAT_UNKNOWN)
        .FirstElement(3)
        .NumElements(5)
        .StructureStride(11 * sizeof(float))
        .CreateGPU(2);

    ID3D12ResourcePtr rootStruct = MakeBuffer().Data(structdata);
    MakeSRV(rootStruct)
        .Format(DXGI_FORMAT_UNKNOWN)
        .FirstElement(3)
        .NumElements(5)
        .StructureStride(11 * sizeof(float))
        .CreateGPU(35);
    ID3D12ResourcePtr rootDummy = MakeBuffer().Data(structdata);

    ID3D12ResourcePtr structBuf2 = MakeBuffer().Size(880).UAV();
    D3D12ViewCreator uavView2 = MakeUAV(structBuf2)
                                    .Format(DXGI_FORMAT_UNKNOWN)
                                    .FirstElement(3)
                                    .NumElements(5)
                                    .StructureStride(11 * sizeof(float));
    D3D12_CPU_DESCRIPTOR_HANDLE uav2cpu = uavView2.CreateClearCPU(11);
    D3D12_GPU_DESCRIPTOR_HANDLE uav2gpu = uavView2.CreateGPU(11);

    // need to create non-structured version for clearing
    uavView2 = MakeUAV(structBuf2).Format(DXGI_FORMAT_R32_UINT);
    uav2cpu = uavView2.CreateClearCPU(9);
    uav2gpu = uavView2.CreateGPU(9);

    // Create resources for MSAA draw
    ID3DBlobPtr vsmsaablob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psmsaablob = Compile(msaaPixel, "main", "ps_5_0");

    ID3D12RootSignaturePtr sigmsaa = MakeSig({});

    ID3D12PipelineStatePtr psomsaa = MakePSO()
                                         .RootSig(sigmsaa)
                                         .InputLayout()
                                         .VS(vsmsaablob)
                                         .PS(psmsaablob)
                                         .SampleCount(4)
                                         .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    ID3D12ResourcePtr vbmsaa = MakeBuffer().Data(DefaultTri);

    ID3D12ResourcePtr msaaTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 8, 8)
                                    .RTV()
                                    .Multisampled(4)
                                    .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE msaaRTV = MakeRTV(msaaTex).CreateCPU(1);

    vsblob = Compile(D3DFullscreenQuadVertex, "main", "vs_4_0");
    psblob = Compile(pixelBlit, "main", "ps_5_0");
    ID3D12RootSignaturePtr blitSig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 1),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 8),
    });
    ID3D12PipelineStatePtr blitpso = MakePSO().RootSig(blitSig).VS(vsblob).PS(psblob);

    vsblob = Compile(vertexSampleVS, "main", "vs_5_0");
    psblob = Compile(vertexSamplePS, "main", "ps_5_0");
    ID3D12RootSignaturePtr vertexSampleSig = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_VERTEX, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 8),
        },
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);
    ID3D12PipelineStatePtr vertexSamplePSO = MakePSO().RootSig(vertexSampleSig).VS(vsblob).PS(psblob);

    // set the NULL descriptors
    UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
      srvdesc.Format = DXGI_FORMAT_R32_FLOAT;
      srvdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvdesc.Buffer.NumElements = 10;

      cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += inc * 20;
      dev->CreateShaderResourceView(NULL, &srvdesc, cpu);
    }

    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
      srvdesc.Format = DXGI_FORMAT_R32_FLOAT;
      srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvdesc.Texture2D.MipLevels = 1;

      cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += inc * 21;
      dev->CreateShaderResourceView(NULL, &srvdesc, cpu);
    }

    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
      uavdesc.Format = DXGI_FORMAT_R32_FLOAT;
      uavdesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uavdesc.Buffer.NumElements = 10;

      cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += inc * 30;
      dev->CreateUnorderedAccessView(NULL, NULL, &uavdesc, cpu);
    }

    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
      uavdesc.Format = DXGI_FORMAT_R32_FLOAT;
      uavdesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

      cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += inc * 31;
      dev->CreateUnorderedAccessView(NULL, NULL, &uavdesc, cpu);
    }

    vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    psblob = Compile(D3DDefaultPixel, "main", "ps_5_0");
    ID3D12RootSignaturePtr bannedSig =
        MakeSig({}, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);
    ID3D12PipelineStatePtr bannedPSO =
        MakePSO().InputLayout().RootSig(bannedSig).VS(vsblob).PS(psblob);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(2);
      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      setMarker(cmd, undefined_tests);

      ID3D12PipelineStatePtr psos[2] = {pso_5_0, pso_5_1};
      float blitOffsets[2] = {0.0f, 4.0f};
      D3D12_RECT scissors[2] = {{0, 0, (int)texDim, 4}, {0, 4, (int)texDim, 8}};
      const char *markers[2] = {"sm_5_0", "sm_5_1"};

      // Clear, draw, and blit to backbuffer twice - once for SM 5.0 and again for SM 5.1
      for(int i = 0; i < 2; ++i)
      {
        OMSetRenderTargets(cmd, {fltRTV}, {});
        ClearRenderTargetView(cmd, fltRTV, {0.2f, 0.2f, 0.2f, 1.0f});

        IASetVertexBuffer(cmd, vb, sizeof(ConstsA2V), 0);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmd->SetGraphicsRootSignature(sig);
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(3, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(4, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootUnorderedAccessView(5, rootDummy->GetGPUVirtualAddress());
        cmd->SetGraphicsRootShaderResourceView(
            6, rootStruct->GetGPUVirtualAddress() + sizeof(float) * 22);

        cmd->SetPipelineState(psos[i]);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)texDim, 4.0f, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, (int)texDim, 4});

        UINT zero[4] = {};
        cmd->ClearUnorderedAccessViewUint(uav1gpu, uav1cpu, rawBuf2, zero, 0, NULL);
        cmd->ClearUnorderedAccessViewUint(uav2gpu, uav2cpu, structBuf2, zero, 0, NULL);

        // Add a marker so we can easily locate this draw
        setMarker(cmd, markers[i]);
        cmd->DrawInstanced(3, numTests, 0, 0);

        ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        OMSetRenderTargets(cmd, {rtv}, {});
        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, scissors[i]);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        cmd->SetGraphicsRootSignature(blitSig);
        cmd->SetPipelineState(blitpso);
        cmd->SetGraphicsRoot32BitConstant(0, *(UINT *)&blitOffsets[i], 0);
        cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->DrawInstanced(4, 1, 0, 0);

        ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_RENDER_TARGET);
      }

      // Render MSAA test
      OMSetRenderTargets(cmd, {msaaRTV}, {});
      ClearRenderTargetView(cmd, msaaRTV, {0.2f, 0.2f, 0.2f, 1.0f});
      IASetVertexBuffer(cmd, vbmsaa, sizeof(DefaultA2V), 0);
      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      cmd->SetGraphicsRootSignature(sigmsaa);
      cmd->SetPipelineState(psomsaa);
      RSSetViewport(cmd, {0.0f, 0.0f, 8.0f, 8.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, 8, 8});

      // Add a marker so we can easily locate this draw
      setMarker(cmd, "MSAA");
      cmd->DrawInstanced(3, 1, 0, 0);

      OMSetRenderTargets(cmd, {fltRTV}, {});
      ClearRenderTargetView(cmd, fltRTV, {0.3f, 0.5f, 0.8f, 1.0f});

      ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

      OMSetRenderTargets(cmd, {rtv}, {});
      RSSetViewport(cmd, {50.0f, 50.0f, 10.0f, 10.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {50, 50, 60, 60});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      cmd->SetGraphicsRootSignature(vertexSampleSig);
      cmd->SetPipelineState(vertexSamplePSO);
      cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      setMarker(cmd, "VertexSample");
      cmd->DrawInstanced(4, 1, 0, 0);

      setMarker(cmd, "BannedSig");
      RSSetViewport(cmd, {60.0f, 60.0f, 10.0f, 10.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {60, 60, 70, 70});
      cmd->SetGraphicsRootSignature(bannedSig);
      cmd->SetPipelineState(bannedPSO);
      cmd->DrawInstanced(3, 1, 0, 0);

      ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();
      Submit({cmd});
      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
