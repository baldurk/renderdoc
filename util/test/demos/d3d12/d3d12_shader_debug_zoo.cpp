/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2025 Baldur Karlsson
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
    float texDim;
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

  std::string shaderTypes = R"EOSHADER(
#if SM_6_6

#define INT32 int32_t
#define UINT32 uint32_t

#if HAS_16BIT_SHADER_OPS 
#define INT16 int16_t
#define UINT16 uint16_t
#define HALF half
#else // #if HAS_16BIT_SHADER_OPS 
#define INT16 int
#define UINT16 uint
#define HALF float
#endif // #if HAS_16BIT_SHADER_OPS 

#if HAS_DOUBLE_SHADER_OPS
#define INT64 int64_t
#define UINT64 uint64_t
#define DOUBLE double
#else // #if HAS_DOUBLE_SHADER_OPS
#define INT64 int
#define UINT64 uint
#define DOUBLE float
#endif // #if HAS_DOUBLE_SHADER_OPS

#else // #if SM_6_6

#define INT32 int
#define UINT32 uint
#define INT16 int
#define UINT16 uint
#define INT64 int
#define UINT64 uint

#define HALF float
#define DOUBLE float
#endif // #if SM_6_6
)EOSHADER";

  std::string common = R"EOSHADER(

struct consts
{
  float3 pos : POSITION;
  float zeroVal : ZERO;
  float oneVal : ONE;
  float negoneVal : NEGONE;
  float texDim : TEXDIM;
};

struct v2f
{
  float4 pos : SV_POSITION;
  float4 s : S;
  float2 zeroVal : ZERO;
  float tinyVal : TINY;
  float oneVal : ONE;
  float negoneVal : NEGONE;
  uint tri : TRIANGLE;
  uint intval : INTVAL;
  row_major float2x3 mat : MAT;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

v2f main(consts IN, uint tri : SV_InstanceID)
{
  v2f OUT = (v2f)0;

  OUT.pos = float4(IN.pos.x + IN.pos.z * float(tri), IN.pos.y, 0.0f, 1);
  // OUT.s.xy : 0 -> 2 : across the triangle x & y, changes per pixel
  OUT.s.x = IN.pos.x + 1.0;
  OUT.s.x *= IN.texDim;
  OUT.s.x -= 1.0;
  OUT.s.x /= 2.0;

  OUT.s.y = IN.pos.y;
  OUT.s.y *= 2.0;
  OUT.s.y += 0.5;
  OUT.s.y = 2.0 - OUT.s.y;
  // OUT.s.zw : large variation in x & y
  OUT.s.zw = (IN.pos.xy + float2(543.0, 213.0)) * (IN.pos.yx + float2(100.0, -113.0));

  OUT.zeroVal = IN.zeroVal.xx;
  OUT.oneVal = IN.oneVal;
  OUT.negoneVal = IN.negoneVal;
  OUT.tri = tri;
  OUT.tinyVal = IN.oneVal * 1.0e-30f;
  OUT.intval = tri + 7;

  OUT.mat[0].x = 1.0;
  OUT.mat[0].y = 2.0;
  OUT.mat[0].z = 3.0;
  OUT.mat[1].x = 4.0;
  OUT.mat[1].y = 5.0;
  OUT.mat[1].z = 6.0;

  return OUT;
}

)EOSHADER";

  std::string pixel = shaderTypes + R"EOSHADER(

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
RWByteAddressBuffer byterwtest2 : register(u3);

Buffer<float> unboundsrv1 : register(t100);
Texture2D<float> unboundsrv2 : register(t101);

RWBuffer<float> unbounduav1 : register(u4);
RWTexture2D<float> unbounduav2 : register(u5);

RWBuffer<float> narrowtypeduav : register(u6);

RWTexture2D<float4> floattexrwtest : register(u7);
RWBuffer<int> intbufrwtest : register(u8);
RWBuffer<int> oneintbufrwtest : register(u9);
RWBuffer<float4> typedrwtest : register(u10);
RWTexture2D<float4> floattex2rwtest : register(u11);

Buffer<float> narrowtypedsrv : register(t102);

Buffer<float4> rgb_srv : register(t103);

SamplerState linearclamp : register(s0);

StructuredBuffer<MyStruct> rootsrv : register(t20);
StructuredBuffer<MyStruct> appendsrv : register(t40);
Texture2D<float> dimtex_edge : register(t41);
#if (SM_6_2 || SM_6_6) && HAS_16BIT_SHADER_OPS 
StructuredBuffer<int16_t> int16srv : register(t42);
#else
Buffer<int> int16srv : register(t43);
#endif

static const int gConstInt = 10;
static const int gConstIntArray[6] = { 1, 2, 3, 4, 5, 6 };
static int gInt = 3;
static int gIntArray[2] = { 5, 6 };

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

    return float4(asfloat(byterotest.Load(z+88).x), asfloat(byterotest.Load(z+92).x),
                  asfloat(byterotest.Load(z+96).x), float(byterotest.Load(z+4096).x));
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
    return asfloat(byterotest.Load4(z+88));
  }
  // 4-uint load out of view bounds
  if(IN.tri == 39)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;

    // test a 4-uint load
    return asfloat(byterotest.Load4(z+96));
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

    byterwtest.Store(z+88, asuint(1.2345f));
    byterwtest.Store(z+92, asuint(9.8765f));
    byterwtest.Store(z+96, asuint(1.81818f));
    byterwtest.Store(z+4096, asuint(5.55555f));

    return float4(asfloat(byterwtest.Load(z2+88).x), asfloat(byterwtest.Load(z2+92).x),
                  asfloat(byterwtest.Load(z2+96).x), float(byterwtest.Load(z2+4096).x));
  }
  // 4-uint store
  if(IN.tri == 43)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest.Store4(z+24, uint4(99, 88, 77, 66));

    return asfloat(byterwtest.Load4(z2+24));
  }
  // 4-uint store crossing view bounds
  if(IN.tri == 44)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest2.Store4(z+88, uint4(99, 88, 77, 66));

    return asfloat(byterwtest2.Load4(z2+88));
  }
  // 4-uint store out of view bounds
  if(IN.tri == 45)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);

    byterwtest2.Store4(z+96, uint4(99, 88, 77, 66));

    return asfloat(byterwtest2.Load4(z2+96));
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
  if(IN.tri == 82)
  {
    uint f16_half = f32tof16(posone*0.5);
    uint f16_one = f32tof16(posone*1.0);
    uint f16_two = f32tof16(posone*2.0);
    return float4(f16tof32(f16_half), f16tof32(f16_one), f16tof32(f16_two), 0.0f);
  }
  if(IN.tri == 83)
  {
    float4 value = float4(posone, posone/3, posone/4, posone/5);
    int2 uv = int2(31,37);
    floattexrwtest[uv] = value;
    return floattexrwtest[uv];
  }
  if(IN.tri == 84)
  {
    return float4(int16srv[0].x, int16srv[1].x, int16srv[2].x, int16srv[3].x);
  }
  if(IN.tri == 85)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 10;
    InterlockedAdd(intbufrwtest[u], value, original);
    InterlockedAdd(intbufrwtest[u], -value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 86)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 20;
    InterlockedAnd(intbufrwtest[u], value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 87)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 30;
    InterlockedOr(intbufrwtest[u], value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 88)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 40;
    InterlockedXor(intbufrwtest[u], value, original);
    InterlockedXor(intbufrwtest[u], value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 89)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 50;
    InterlockedMin(intbufrwtest[u], value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 90)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 60;
    InterlockedMax(intbufrwtest[u], value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 91)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 70;
    InterlockedExchange(intbufrwtest[u], value, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 92)
  {
    int value = IN.tri;
    int original;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 80;
    InterlockedCompareExchange(intbufrwtest[u], value, value+1, original);
    return intbufrwtest[u];
  }
  if(IN.tri == 93)
  {
    int value = IN.tri;
    int u = mad(3, (IN.tri - 85), 17);
    intbufrwtest[u] = 90;
    InterlockedCompareStore(intbufrwtest[u], value, value+1);
    return intbufrwtest[u];
  }
#if SM_6_6
  if(IN.tri == 94)
  {
    uint a = IN.tri - 94 + 0x01020304;
    uint b = IN.tri - 94 + 0x05060708;
    uint c = IN.tri - 94 + 0x090a0b0c;
    uint res = dot4add_i8packed(a, b, c);
    return float4(res & 0xFF, (res >> 8) & 0xFF, (res >> 16) & 0xFF, (res >> 24) & 0xFF);
  }
  if(IN.tri == 95)
  {
    uint a = IN.tri - 94 + 0x01020304;
    uint b = IN.tri - 94 + 0x05060708;
    uint c = IN.tri - 94 + 0x090a0b0c;
    uint res = dot4add_u8packed(a, b, c);
    return float4(res & 0xFF, (res >> 8) & 0xFF, (res >> 16) & 0xFF, (res >> 24) & 0xFF);
  }
  if(IN.tri == 96)
  {
    vector<HALF, 2> a = {IN.tri - 96 + 0.25f, IN.tri - 96 + 0.5f};
    vector<HALF, 2> b = {IN.tri - 96 + 0.5f, IN.tri - 96 + 0.25f};
    float c = IN.tri - 96 + 0.3f;
    return dot2add(a, b, c);
  }
  if(IN.tri == 97)
  {
    int val = IN.tri - 97;
    int4 raw = int4(val-200, val+1, val+200, val+3);
    uint packed = pack_clamp_u8(raw);
    int4 unpacked = unpack_s8s32(packed);
    return float4(unpacked.x, unpacked.y, unpacked.z, unpacked.w);
  }
  if(IN.tri == 98)
  {
    int val = IN.tri - 97;
    int4 raw = int4(val, val+100, val+200, val+300);
    int packed = pack_s8(raw);
    uint4 unpacked = unpack_u8u32(packed);
    return float4(unpacked.x, unpacked.y, unpacked.z, unpacked.w);
  }
#endif // #if SM_6_6
  if(IN.tri == 99)
  {
    // use this to ensure the compiler doesn't know we're using fixed locations
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);
    oneintbufrwtest[z] = 10;
    return oneintbufrwtest[z2];
  }
  // test UAV loads and stores only write the data they should
  if(IN.tri == 100)
  {
    // typed UAVs have to write all components so this is a fairly degenerate test
    typedrwtest[uint(zero) + 20] = 9.99999f.xxxx;
    return typedrwtest[uint(posone) + 19];
  }
  if(IN.tri == 101)
  {
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);
    uint z3 = uint(posone) - 1;

    // fill the first component, to ensure we return the real result and not a trashed-zero
    byterwtest.Store(z3+48, asuint(1.1f));

    // unaligned raw store of less than float4
    byterwtest.Store3(z+52, asuint(float3(9.9f, 8.8f, 7.7f)));

    return asfloat(byterwtest.Load4(z2+48));
  }
  if(IN.tri == 102)
  {
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);
    uint z3 = uint(posone) - 1;

    // fill the last component, to ensure we return the real result and not a trashed-zero
    byterwtest.Store(z3+44, asuint(1.1f));

    // unaligned raw store of less than float4
    byterwtest.Store3(z+32, asuint(float3(9.9f, 8.8f, 7.7f)));

    return asfloat(byterwtest.Load4(z2+32));
  }
  if(IN.tri == 103)
  {
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);
    uint z3 = uint(posone) - 1;

    // fill the last component, to ensure we return the real result and not a trashed-zero
    structrwtest[z+4].b.w = 1.1f;

    // aligned store of float3
    structrwtest[z3+4].b.xzy = float3(1.234f, 5.678f, 9.999f);

    return structrwtest[z2+4].b;
  }
  if(IN.tri == 104)
  {
    uint z = intval - IN.tri - 7;
    uint z2 = uint(zero);
    uint z3 = uint(posone) - 1;

    // fill the first component, to ensure we return the real result and not a trashed-zero
    structrwtest[z+5].b.x = 1.1f;

    // unaligned store of float3
    structrwtest[z3+5].b.wzy = float3(1.234f, 5.678f, 9.999f);

    return structrwtest[z2+5].b;
  }
  if(IN.tri == 105)
  {
    // idx = 0
    int idx = intval - IN.tri - 7;
    return float4(gConstInt, gConstIntArray[idx+5], gConstIntArray[idx+1], gConstIntArray[idx+4]);
  }
  if(IN.tri == 106)
  {
    // idx = 0
    int idx = intval - IN.tri - 7;
    int prev = gInt;
    gInt += (idx+1) + IN.s.x + IN.s.y;
    gIntArray[idx] = gInt;
    return float4(prev, gInt, gIntArray[idx], gIntArray[idx+1]);
  }
  if(IN.tri == 107)
  {
    float4 value = float4(posone, posone/3, posone/4, posone/5);
    int2 uv = int2(31,37);
    floattex2rwtest[uv] = value;
    return floattex2rwtest[uv];
  }
  if(IN.tri == 108)
  {
    float4 Color = float4(0,0,0,0);
    // this is intended to test triggering a mixture of GPU math and GPU sample ops
    float2 coord = float2(zero + 0.5, zero + 0.15);
    if (IN.s.x % 2 == 0)
    {
      Color = smiley.SampleLevel(linearclamp, coord, float(0));
      for (int i = 0; i < 100; i++)
      {
        Color += smiley.SampleLevel(linearclamp, coord, float(i));
      }
    }
    else
    {
      Color = float4(pow(abs(posone*2.5f), posone*1.3f), pow(abs(posone*2.5f), posone*0.45f),
                     pow(abs(posone*2.5f), posone*0.9f), pow(abs(posone*0.9f), posone*8.5f));
      for (int i = 0; i < 100; i++)
      {
        float4 value = float4(pow(abs(posone*2.5f+float(i)), posone*1.3f), pow(abs(posone*2.5f), posone*0.45f),
                              pow(abs(posone*2.5f), posone*0.9f), pow(abs(posone*1.3), posone*8.5f));
        Color += value / 100.0;
      }
    }
    return Color;
  }
  return float4(0.4f, 0.4f, 0.4f, 0.4f);
}
)EOSHADER";

  std::string noResourcesPixel = shaderTypes + R"EOSHADER(

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
  {
    // IN.s.xy : 0/1/2 : across the triangle in x & y
    float2 s = IN.s.xy;
    return float4(ddx(s.x), ddy(s.y), s.x, s.y);
  }
  if(IN.tri == 1)
  {
    // IN.s.wz : large variation across the triangle in x & y
    float2 s = IN.s.zw;
    return float4(ddx(s.x), ddy(s.y), s.x, s.y);
  }
  if(IN.tri == 2)
  {
    // IN.s : 0/1/2 : across the triangle in x & y
    float2 s = IN.s.xy;
    if (s.x > 0.5)
      discard;
    if (s.y > 0.5)
      discard;
    s *= posone * float2(0.55f, 0.48f);
    return float4(ddx(s.x), ddy(s.y), s.x, s.y);
  }
  if(IN.tri == 3)
  {
    float4 col = float4(0.0, 0.0, 0.0, 0.0);
    col.x += IN.mat[0].x;
    col.y += IN.mat[0].y;
    col.z += IN.mat[0].z;
    col.x += IN.mat[1].x;
    col.y += IN.mat[1].y;
    col.z += IN.mat[1].z;
    return col;
  }
  if(IN.tri == 4)
  {
    float4 Color = float4(0,0,0,0);
    float floatA = IN.tri/100.0 + 1.5f;
    float floatB = IN.tri/100.0 + 1.7f;
    float floatC = IN.tri/100.0 + 2.5f;
    DOUBLE doubleA = (DOUBLE)floatA; 
    DOUBLE doubleB = (DOUBLE)floatB;
    DOUBLE doubleC = (DOUBLE)floatC;
    HALF halfA = (HALF)floatA;
    HALF halfB = (HALF)1.0;
    HALF halfC = (HALF)floatC;

    HALF halfFma = mad(halfA, halfB, halfC);
    float floatFma = mad(floatA, floatB, floatC);
    DOUBLE doubleFma = mad(doubleA, doubleB, doubleC);
    Color.x = floatFma * 1000.0;
    Color.y = (float)halfFma * 1000.0;
    Color.z = (float)doubleFma * 1000.0;
    return Color;
  }
  if(IN.tri == 5)
  {
    float4 Color = float4(0,0,0,0);
    float floatA = IN.tri/100.0 + 1.5f;
    float floatB = IN.tri/100.0 + 1.7f;
    DOUBLE doubleA = (DOUBLE)floatA; 
    DOUBLE doubleB = (DOUBLE)floatB;
    HALF halfA = (HALF)floatA;
    HALF halfB = (HALF)floatB;

    HALF half_val = min(halfA, halfB);
    float float_val = min(floatA, floatB);
    DOUBLE double_val = min(doubleA, doubleB);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    Color.z = (float)double_val * 1000.0;
    return Color;
  }
  if(IN.tri == 6)
  {
    float4 Color = float4(0,0,0,0);
    float floatA = IN.tri/100.0 + 1.5f;
    float floatB = IN.tri/100.0 + 1.7f;
    DOUBLE doubleA = (DOUBLE)floatA; 
    DOUBLE doubleB = (DOUBLE)floatB;
    HALF halfA = (HALF)floatA;
    HALF halfB = (HALF)floatB;

    HALF half_val = max(halfA, halfB);
    float float_val = max(floatA, floatB);
    DOUBLE double_val = max(doubleA, doubleB);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    Color.z = (float)double_val * 1000.0;
    return Color;
  }
  if(IN.tri == 7)
  {
    float4 Color = float4(0,0,0,0);
    float floatA = IN.tri/100.0 + 1.5f;
    DOUBLE doubleA = (DOUBLE)floatA; 
    HALF halfA = (HALF)floatA;

    HALF half_val = abs(halfA);
    float float_val = abs(floatA);
    DOUBLE double_val = abs(doubleA);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    Color.z = (float)double_val * 1000.0;
    return Color;
  }
  if(IN.tri == 8)
  {
    float4 Color = float4(0,0,0,0);
    float floatA = IN.tri/100.0 + 0.5f;
    HALF halfA = (HALF)floatA;

    HALF half_val = frac(halfA);
    float float_val = frac(floatA);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    return Color;
  }
  if(IN.tri == 9)
  {
    float4 Color = float4(0,0,0,0);
    float floatA = IN.tri/100.0 + 1.5f;
    DOUBLE doubleA = (DOUBLE)floatA; 
    HALF halfA = (HALF)floatA;

    HALF half_val = saturate(halfA);
    float float_val = saturate(floatA);
    DOUBLE double_val = saturate(doubleA);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    Color.z = (float)double_val * 1000.0;
    return Color;
  }
  if(IN.tri == 10)
  {
    float4 Color = float4(0,0,0,0);
    float2 floatA = float2(IN.tri/100.0 + 1.5f, IN.tri/100.0 + 1.7f);
    float2 floatB = float2(IN.tri/100.0 - 1.5f, IN.tri/100.0 - 1.7f);
    vector<HALF, 2> halfA = {IN.tri + 1.5f, IN.tri + 1.7f};
    vector<HALF, 2> halfB = {IN.tri - 1.5f, IN.tri - 1.7f};

    HALF half_val = dot(halfA, halfB);
    float float_val = dot(floatA, floatB);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    return Color;
  }
  if(IN.tri == 11)
  {
    float4 Color = float4(0,0,0,0);
    float3 floatA = float3(IN.tri/100.0 + 1.5f, IN.tri/100.0 + 1.7f, IN.tri/100.0 + 2.7f);
    float3 floatB = float3(IN.tri/100.0 - 1.5f, IN.tri/100.0 + 1.7f, IN.tri/100.0 - 2.7f);
    vector<HALF, 3> halfA = {IN.tri/100.0 + 1.5f, IN.tri/100.0 + 1.7f, IN.tri/100.0 + 2.7f};
    vector<HALF, 3> halfB = {IN.tri/100.0 - 1.5f, IN.tri/100.0 - 1.7f, IN.tri/100.0 - 2.7f};

    HALF half_val = dot(halfA, halfB);
    float float_val = dot(floatA, floatB);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    return Color;
  }
  if(IN.tri == 12)
  {
    float4 Color = float4(0,0,0,0);
    float4 floatA = float4(IN.tri/100.0 + 1.5f, IN.tri/100.0 + 1.7f, IN.tri/100.0 + 2.7f, IN.tri/100.0 + 3.7f);
    float4 floatB = float4(IN.tri/100.0 - 1.5f, IN.tri/100.0 - 1.7f, IN.tri/100.0 - 2.7f, IN.tri/100.0 - 3.7f);
    vector<HALF, 4> halfA = {IN.tri + 1.5f, IN.tri + 1.7f, IN.tri + 2.7f, IN.tri + 3.7f};
    vector<HALF, 4> halfB = {IN.tri - 1.5f, IN.tri - 1.7f, IN.tri - 2.7f, IN.tri - 3.7f};

    HALF half_val = dot(halfA, halfB);
    float float_val = dot(floatA, floatB);
    Color.x = float_val * 1000.0;
    Color.y = (float)half_val * 1000.0;
    return Color;
  }
  if(IN.tri == 13)
  {
    float4 Color = float4(0,0,0,0);
    INT32 int_A = IN.tri/100.0 + 1.5f;
    INT32 int_B = IN.tri/100.0 + 1.7f;
    INT64 slong_A = (INT64)int_A; 
    INT64 slong_B = (INT64)int_B;
    INT16 short_A = (INT16)int_A;
    INT16 short_B = (INT16)int_B;

    INT16 short_val = min(short_A, short_B);
    int int_val = min(int_A, int_B);
    INT64 slong_val = min(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 14)
  {
    float4 Color = float4(0,0,0,0);
    INT32 int_A = IN.tri/100.0 + 1.5f;
    INT32 int_B = IN.tri/100.0 + 1.7f;
    INT64 slong_A = (INT64)int_A; 
    INT64 slong_B = (INT64)int_B;
    INT16 short_A = (INT16)int_A;
    INT16 short_B = (INT16)int_B;

    INT16 short_val = max(short_A, short_B);
    int int_val = max(int_A, int_B);
    INT64 slong_val = max(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 15)
  {
    float4 Color = float4(0,0,0,0);
    INT32 int_A = IN.tri/100.0 + 1.5f;
    INT32 int_B = IN.tri/100.0 + 1.7f;
    INT64 slong_A = (INT64)int_A; 
    INT64 slong_B = (INT64)int_B;
    INT16 short_A = (INT16)int_A;
    INT16 short_B = (INT16)int_B;

    INT16 short_val = short_A * short_B;
    int int_val = int_A * int_B;
    INT64 slong_val = slong_A * slong_B;
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 16)
  {
    float4 Color = float4(0,0,0,0);
    INT32 int_A = IN.tri/100.0 + 1.5f;
    INT32 int_B = IN.tri/100.0 + 1.7f;
    INT64 slong_A = (INT64)int_A; 
    INT64 slong_B = (INT64)int_B;
    INT16 short_A = (INT16)int_A;
    INT16 short_B = (INT16)int_B;

    INT16 short_val = short_A / short_B;
    int int_val = int_A / int_B;
    INT64 slong_val = slong_A / slong_B;
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 17)
  {
    float4 Color = float4(0,0,0,0);
    INT32 int_A = IN.tri/100.0 + 1.5f;
    INT32 int_B = IN.tri/100.0 + 1.7f;
    INT32 int_C = IN.tri/100.0 + 2.7f;
    INT64 slong_A = (INT64)int_A; 
    INT64 slong_B = (INT64)int_B;
    INT64 slong_C = (INT64)int_C;
    INT16 short_A = (INT16)int_A;
    INT16 short_B = (INT16)int_B;
    INT16 short_C = (INT16)int_C;

    INT16 short_val = mad(short_A, short_B, short_C);
    int int_val = mad(int_A, int_B, int_C);
    INT64 slong_val = mad(slong_A, slong_B, slong_C);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 18)
  {
    float4 Color = float4(0,0,0,0);
    vector<INT32,2> int_A = {IN.tri/100.0 + 1.5f, IN.tri - 1.5f};
    vector<INT32,2> int_B = {IN.tri/100.0 + 1.7f, IN.tri - 1.7f};
    vector<INT64,2> slong_A = {(INT64)int_A.x, (INT64)int_A.y}; 
    vector<INT64,2> slong_B = {(INT64)int_B.x, (INT64)int_B.y};
    vector<INT16,2> short_A = {(INT16)int_A.x, (INT16)int_A.y};
    vector<INT16,2> short_B = {(INT16)int_B.x, (INT16)int_B.y};

    INT16 short_val = dot(short_A, short_B);
    int int_val = dot(int_A, int_B);
    INT64 slong_val = dot(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 19)
  {
    float4 Color = float4(0,0,0,0);
    vector<INT32,3> int_A = {IN.tri/100.0 + 1.5f, IN.tri - 1.5f, IN.tri - 2.5f};
    vector<INT32,3> int_B = {IN.tri/100.0 + 1.7f, IN.tri - 1.7f, IN.tri + 2.5f};
    vector<INT64,3> slong_A = {(INT64)int_A.x, (INT64)int_A.y, (INT64)int_A.z};
    vector<INT64,3> slong_B = {(INT64)int_B.x, (INT64)int_B.y, (INT64)int_B.z};
    vector<INT16,3> short_A = {(INT16)int_A.x, (INT16)int_A.y, (INT16)int_A.z};
    vector<INT16,3> short_B = {(INT16)int_B.x, (INT16)int_B.y, (INT16)int_B.z};

    INT16 short_val = dot(short_A, short_B);
    int int_val = dot(int_A, int_B);
    INT64 slong_val = dot(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 20)
  {
    float4 Color = float4(0,0,0,0);
    vector<INT32,4> int_A = {IN.tri/100.0 + 1.5f, IN.tri - 1.5f, IN.tri - 2.5f, IN.tri + 3.7f};
    vector<INT32,4> int_B = {IN.tri/100.0 + 1.7f, IN.tri - 1.7f, IN.tri + 2.5f, IN.tri -3.3f};
    vector<INT64,4> slong_A = {(INT64)int_A.x, (INT64)int_A.y, (INT64)int_A.z, (INT64)int_A.w};
    vector<INT64,4> slong_B = {(INT64)int_B.x, (INT64)int_B.y, (INT64)int_B.z, (INT64)int_B.w};
    vector<INT16,4> short_A = {(INT16)int_A.x, (INT16)int_A.y, (INT16)int_A.z, (INT16)int_A.w};
    vector<INT16,4> short_B = {(INT16)int_B.x, (INT16)int_B.y, (INT16)int_B.z, (INT16)int_B.w};

    INT16 short_val = dot(short_A, short_B);
    int int_val = dot(int_A, int_B);
    INT64 slong_val = dot(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 21)
  {
    float4 Color = float4(0,0,0,0);
    UINT32 int_A = IN.tri/100.0 + 1.5f;
    UINT32 int_B = IN.tri/100.0 + 1.7f;
    UINT64 slong_A = (UINT64)int_A; 
    UINT64 slong_B = (UINT64)int_B;
    UINT16 short_A = (UINT16)int_A;
    UINT16 short_B = (UINT16)int_B;

    UINT16 short_val = min(short_A, short_B);
    uint int_val = min(int_A, int_B);
    UINT64 slong_val = min(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 22)
  {
    float4 Color = float4(0,0,0,0);
    UINT32 int_A = IN.tri/100.0 + 1.5f;
    UINT32 int_B = IN.tri/100.0 + 1.7f;
    UINT64 slong_A = (UINT64)int_A; 
    UINT64 slong_B = (UINT64)int_B;
    UINT16 short_A = (UINT16)int_A;
    UINT16 short_B = (UINT16)int_B;

    UINT16 short_val = max(short_A, short_B);
    uint int_val = max(int_A, int_B);
    UINT64 slong_val = max(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 23)
  {
    float4 Color = float4(0,0,0,0);
    UINT32 int_A = IN.tri/100.0 + 1.5f;
    UINT32 int_B = IN.tri/100.0 + 1.7f;
    UINT64 slong_A = (UINT64)int_A; 
    UINT64 slong_B = (UINT64)int_B;
    UINT16 short_A = (UINT16)int_A;
    UINT16 short_B = (UINT16)int_B;

    UINT16 short_val = short_A * short_B;
    uint int_val = int_A * int_B;
    UINT64 slong_val = slong_A * slong_B;
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 24)
  {
    float4 Color = float4(0,0,0,0);
    UINT32 int_A = IN.tri/100.0 + 1.5f;
    UINT32 int_B = IN.tri/100.0 + 1.7f;
    UINT64 slong_A = (UINT64)int_A; 
    UINT64 slong_B = (UINT64)int_B;
    UINT16 short_A = (UINT16)int_A;
    UINT16 short_B = (UINT16)int_B;

    UINT16 short_val = short_A / short_B;
    uint int_val = int_A / int_B;
    UINT64 slong_val = slong_A / slong_B;
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 25)
  {
    float4 Color = float4(0,0,0,0);
    UINT32 int_A = IN.tri/100.0 + 1.5f;
    UINT32 int_B = IN.tri/100.0 + 1.7f;
    UINT32 int_C = IN.tri/100.0 + 2.7f;
    UINT64 slong_A = (UINT64)int_A; 
    UINT64 slong_B = (UINT64)int_B;
    UINT64 slong_C = (UINT64)int_C;
    UINT16 short_A = (UINT16)int_A;
    UINT16 short_B = (UINT16)int_B;
    UINT16 short_C = (UINT16)int_C;

    UINT16 short_val = mad(short_A, short_B, short_C);
    uint int_val = mad(int_A, int_B, int_C);
    UINT64 slong_val = mad(slong_A, slong_B, slong_C);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 26)
  {
    float4 Color = float4(0,0,0,0);
    vector<UINT32,2> int_A = {IN.tri/100.0 + 1.5f, IN.tri - 1.5f};
    vector<UINT32,2> int_B = {IN.tri/100.0 + 1.7f, IN.tri - 1.7f};
    vector<UINT64,2> slong_A = {(UINT64)int_A.x, (UINT64)int_A.y}; 
    vector<UINT64,2> slong_B = {(UINT64)int_B.x, (UINT64)int_B.y};
    vector<UINT16,2> short_A = {(UINT16)int_A.x, (UINT16)int_A.y};
    vector<UINT16,2> short_B = {(UINT16)int_B.x, (UINT16)int_B.y};

    UINT16 short_val = dot(short_A, short_B);
    uint int_val = dot(int_A, int_B);
    UINT64 slong_val = dot(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 27)
  {
    float4 Color = float4(0,0,0,0);
    vector<UINT32,3> int_A = {IN.tri/100.0 + 1.5f, IN.tri - 1.5f, IN.tri - 2.5f};
    vector<UINT32,3> int_B = {IN.tri/100.0 + 1.7f, IN.tri - 1.7f, IN.tri + 2.5f};
    vector<UINT64,3> slong_A = {(UINT64)int_A.x, (UINT64)int_A.y, (UINT64)int_A.z};
    vector<UINT64,3> slong_B = {(UINT64)int_B.x, (UINT64)int_B.y, (UINT64)int_B.z};
    vector<UINT16,3> short_A = {(UINT16)int_A.x, (UINT16)int_A.y, (UINT16)int_A.z};
    vector<UINT16,3> short_B = {(UINT16)int_B.x, (UINT16)int_B.y, (UINT16)int_B.z};

    UINT16 short_val = dot(short_A, short_B);
    uint int_val = dot(int_A, int_B);
    UINT64 slong_val = dot(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }
  if(IN.tri == 28)
  {
    float4 Color = float4(0,0,0,0);
    vector<UINT32,4> int_A = {IN.tri/100.0 + 1.5f, IN.tri - 1.5f, IN.tri - 2.5f, IN.tri + 3.7f};
    vector<UINT32,4> int_B = {IN.tri/100.0 + 1.7f, IN.tri - 1.7f, IN.tri + 2.5f, IN.tri -3.3f};
    vector<UINT64,4> slong_A = {(UINT64)int_A.x, (UINT64)int_A.y, (UINT64)int_A.z, (UINT64)int_A.w};
    vector<UINT64,4> slong_B = {(UINT64)int_B.x, (UINT64)int_B.y, (UINT64)int_B.z, (UINT64)int_B.w};
    vector<UINT16,4> short_A = {(UINT16)int_A.x, (UINT16)int_A.y, (UINT16)int_A.z, (UINT16)int_A.w};
    vector<UINT16,4> short_B = {(UINT16)int_B.x, (UINT16)int_B.y, (UINT16)int_B.z, (UINT16)int_B.w};

    UINT16 short_val = dot(short_A, short_B);
    uint int_val = dot(int_A, int_B);
    UINT64 slong_val = dot(slong_A, slong_B);
    Color.x = (float)int_val;
    Color.y = (float)short_val;
    Color.z = (float)slong_val;
    return Color;
  }

  return float4(0.4f, 0.4f, 0.4f, 0.4f);
};
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

  std::string msaaPixel61 = R"EOSHADER(

struct v2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

float4 main(v2f IN, uint samp : SV_SampleIndex, float3 bary : SV_Barycentrics) : SV_Target0 
{
  float2 uvCentroid = EvaluateAttributeCentroid(IN.uv);
  float2 uvSamp0 = EvaluateAttributeAtSample(IN.uv, 0) - IN.uv;
  float2 uvSampThis = EvaluateAttributeAtSample(IN.uv, samp) - IN.uv;
  float2 uvOffset = EvaluateAttributeSnapped(IN.uv, int2(1, 1));

  float x = (uvCentroid.x + uvCentroid.y) * 0.5f;
  float y = (uvSamp0.x + uvSamp0.y) * 0.5f;
  float z = (uvSampThis.x + uvSampThis.y) * 0.5f;
  float w = (uvOffset.x + uvOffset.y) * 0.5f;
  w += x * bary.x + y * bary.y + z * bary.z;

  // Test sampleinfo with a MSAA rasterizer
  uint numSamples = GetRenderTargetSampleCount();
  float2 pos = GetRenderTargetSamplePosition(samp);

  return float4(x + pos.x, y + pos.y, z + (float)numSamples, w);
}

)EOSHADER";

  std::string computeMain = shaderTypes + R"EOSHADER(

// error X3556: integer divides may be much slower, try using uints if possible.
// we want to do this on purpose
#pragma warning( disable : 3556 )

cbuffer consts : register(b0)
{
  bool boolX;
  uint intY;
  float floatZ;
  DOUBLE doubleX;
};

cbuffer packed_consts : register(b1)
{
  uint col1z : packoffset(c1.z);
  uint col2w : packoffset(c2.w);
};

RWStructuredBuffer<uint4> bufIn : register(u0);
RWStructuredBuffer<uint4> bufOut : register(u1);

struct TestStruct
{
  uint3 a;
  uint3 b;
};

groupshared int gsmInt;
groupshared TestStruct gsmStruct[64];
groupshared int gsmIntArray[128];
groupshared int gsmInt2DArray[2][1024];

[numthreads(1,1,1)]
void main(int3 inTestIndex : SV_GroupID)
{
  // Only want the workgroups (*,1,0) to output results
  if ((inTestIndex.y != 1) || (inTestIndex.z != 0))
    return;

  int testIndex = inTestIndex.x;
  int ZERO = floor(testIndex/(testIndex+1.0e-6f));
  int ONE = ZERO + 1;

  int4 testResult = 123;
  gsmInt = testIndex;
  gsmStruct[gsmInt].a = inTestIndex;
  if (testIndex == 0)
  {
    testResult = bufOut[0];
    testResult.x += bufIn[0].x * (uint)boolX;
    testResult.y += bufIn[0].y * (uint)intY;
    testResult.z += bufIn[0].z * (uint)floatZ;
    testResult.w += bufIn[0].w * (uint)doubleX;
  }
  else if (testIndex == 1)
  {
    gsmStruct[gsmInt*4].a = inTestIndex;
    int idx = 128 - gsmInt - 1;
    gsmIntArray[idx] = testIndex;
    gsmInt2DArray[ZERO][idx] = testIndex;
    gsmInt2DArray[ONE][idx] = testIndex;
    testResult.x = gsmIntArray[idx + ZERO];
    testResult.y = testIndex;
    testResult.z = gsmStruct[gsmInt * 4].a.y;
    testResult.w = gsmInt2DArray[ZERO][idx] + gsmInt2DArray[ONE][idx];
  }
  else if (testIndex == 2)
  {
    testResult = bufOut[0];
    testResult.x += bufIn[0].x * (uint)col1z;
    testResult.y += bufIn[0].y * (uint)col2w;
  }
  else
  {
    testResult.x = inTestIndex.x;
  }
  GroupMemoryBarrierWithGroupSync();
  bufOut[gsmInt] = testResult;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    bool supportSM60 = (m_HighestShaderModel >= D3D_SHADER_MODEL_6_0) && m_DXILSupport;
    bool supportSM61 = (m_HighestShaderModel >= D3D_SHADER_MODEL_6_1) && m_DXILSupport;
    bool supportSM62 = (m_HighestShaderModel >= D3D_SHADER_MODEL_6_2) && m_DXILSupport;
    bool supportSM66 = (m_HighestShaderModel >= D3D_SHADER_MODEL_6_6) && m_DXILSupport;
    TEST_ASSERT(!supportSM62 || supportSM60, "SM 6.2 requires SM 6.0 support");
    TEST_ASSERT(!supportSM66 || supportSM62, "SM 6.6 requires SM 6.2 support");

    std::string shaderDefines = "";
    if(opts4.Native16BitShaderOpsSupported)
      shaderDefines += "#define HAS_16BIT_SHADER_OPS 1\n";
    else
      shaderDefines += "#define HAS_16BIT_SHADER_OPS 0\n";
    if(opts.DoublePrecisionFloatShaderOps)
      shaderDefines += "#define HAS_DOUBLE_SHADER_OPS 1\n";
    else
      shaderDefines += "#define HAS_DOUBLE_SHADER_OPS 0\n";

    size_t lastTest = pixel.rfind("IN.tri == ");
    lastTest += sizeof("IN.tri == ") - 1;

    const uint32_t numResTests = atoi(pixel.c_str() + lastTest) + 1;

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

    lastTest = noResourcesPixel.rfind("IN.tri == ");
    lastTest += sizeof("IN.tri == ") - 1;
    const uint32_t numNoResTests = atoi(noResourcesPixel.c_str() + lastTest) + 1;

    lastTest = computeMain.rfind("testIndex == ");
    lastTest += sizeof("testIndex == ") - 1;
    const uint32_t numComputeTests = atoi(computeMain.c_str() + lastTest) + 1;

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
    inputLayout.push_back({
        "TEXDIM",
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

    D3D12_DESCRIPTOR_RANGE1 multiRanges[4] = {
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
        {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            2,
            42,
            0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            42,
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
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 3, 10),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 100, 5, 20),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4, 5, 30),
            multiRangeParam,
            uavParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 21),
            srvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 20),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 9, 3, 100),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);

    const int numShaderModels = 5;    // 5.0, 5.1, 6.0, 6.2, 6.6
    ID3D12PipelineStatePtr psos[numShaderModels * 2] = {};

    ID3DBlobPtr vs5blob = Compile(common + vertex, "main", "vs_5_0");

    psos[0] = MakePSO()
                  .RootSig(sig)
                  .InputLayout(inputLayout)
                  .VS(vs5blob)
                  .PS(Compile(common + shaderDefines + pixel, "main", "ps_5_0",
                              CompileOptionFlags::SkipOptimise))
                  .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    psos[0]->SetName(L"ps_5_0");
    psos[1] =
        MakePSO()
            .RootSig(sig)
            .InputLayout(inputLayout)
            .VS(vs5blob)
            .PS(Compile(common + shaderDefines + pixel, "main", "ps_5_0", CompileOptionFlags::None))
            .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    psos[1]->SetName(L"ps_5_0_opt");

    // Recompile the same PS with SM 5.1 to test shader debugging with the different bytecode
    psos[2] = MakePSO()
                  .RootSig(sig)
                  .InputLayout(inputLayout)
                  .VS(vs5blob)
                  .PS(Compile(common + "\n#define SM_5_1 1\n" + shaderDefines + pixel, "main",
                              "ps_5_1", CompileOptionFlags::SkipOptimise))
                  .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    psos[2]->SetName(L"ps_5_1");
    psos[3] = MakePSO()
                  .RootSig(sig)
                  .InputLayout(inputLayout)
                  .VS(vs5blob)
                  .PS(Compile(common + "\n#define SM_5_1 1\n" + shaderDefines + pixel, "main",
                              "ps_5_1", CompileOptionFlags::None))
                  .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    psos[3]->SetName(L"ps_5_1_opt");

    // Recompile with SM 6.0, SM 6.2 and SM 6.6
    const uint32_t compileOptions = (opts4.Native16BitShaderOpsSupported)
                                        ? CompileOptionFlags::Enable16BitTypes
                                        : CompileOptionFlags::None;
    if(supportSM60)
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_0");
      psos[4] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_0 1\n" + shaderDefines + pixel, "main",
                                "ps_6_0", CompileOptionFlags::SkipOptimise))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[4]->SetName(L"ps_6_0");
      psos[5] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_0 1\n" + shaderDefines + pixel, "main",
                                "ps_6_0", CompileOptionFlags::None))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[5]->SetName(L"ps_6_0_opt");
    }
    if(supportSM62)
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_2");
      psos[6] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_2 1\n" + shaderDefines + pixel, "main",
                                "ps_6_2", compileOptions | CompileOptionFlags::SkipOptimise))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[6]->SetName(L"ps_6_2");
      psos[7] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_2 1\n" + shaderDefines + pixel, "main",
                                "ps_6_2", compileOptions))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[7]->SetName(L"ps_6_2_opt");
    }
    if(supportSM66)
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_6");
      psos[8] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_6 1\n" + shaderDefines + pixel, "main",
                                "ps_6_6", compileOptions | CompileOptionFlags::SkipOptimise))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[8]->SetName(L"ps_6_6");
      psos[9] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_6 1\n" + shaderDefines + pixel, "main",
                                "ps_6_6", compileOptions))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[9]->SetName(L"ps_6_6_opt");
    }

    ID3D12PipelineStatePtr noResPSOs[numShaderModels * 2] = {};
    noResPSOs[0] = MakePSO()
                       .RootSig(sig)
                       .InputLayout(inputLayout)
                       .VS(vs5blob)
                       .PS(Compile(common + noResourcesPixel, "main", "ps_5_0",
                                   CompileOptionFlags::SkipOptimise))
                       .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    noResPSOs[0]->SetName(L"ps_5_0");
    noResPSOs[1] =
        MakePSO()
            .RootSig(sig)
            .InputLayout(inputLayout)
            .VS(vs5blob)
            .PS(Compile(common + noResourcesPixel, "main", "ps_5_0", CompileOptionFlags::None))
            .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    noResPSOs[1]->SetName(L"ps_5_0_opt");

    // Recompile the same PS with SM 5.1 to test shader debugging with the different bytecode
    noResPSOs[2] = MakePSO()
                       .RootSig(sig)
                       .InputLayout(inputLayout)
                       .VS(vs5blob)
                       .PS(Compile(common + "\n#define SM_5_1 1\n" + noResourcesPixel, "main",
                                   "ps_5_1", CompileOptionFlags::SkipOptimise))
                       .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    noResPSOs[2]->SetName(L"ps_5_1");
    noResPSOs[3] = MakePSO()
                       .RootSig(sig)
                       .InputLayout(inputLayout)
                       .VS(vs5blob)
                       .PS(Compile(common + "\n#define SM_5_1 1\n" + noResourcesPixel, "main",
                                   "ps_5_1", CompileOptionFlags::None))
                       .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    noResPSOs[3]->SetName(L"ps_5_1_opt");

    // Recompile with SM 6.0, SM 6.2 and SM 6.6
    if(supportSM60)
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_0");
      noResPSOs[4] =
          MakePSO()
              .RootSig(sig)
              .InputLayout(inputLayout)
              .VS(vsblob)
              .PS(Compile(common + "\n#define SM_6_0 1\n" + shaderDefines + noResourcesPixel,
                          "main", "ps_6_0", CompileOptionFlags::SkipOptimise))
              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      noResPSOs[4]->SetName(L"ps_6_0");
      noResPSOs[5] =
          MakePSO()
              .RootSig(sig)
              .InputLayout(inputLayout)
              .VS(vsblob)
              .PS(Compile(common + "\n#define SM_6_0 1\n" + shaderDefines + noResourcesPixel,
                          "main", "ps_6_0", CompileOptionFlags::None))
              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      noResPSOs[5]->SetName(L"ps_6_0_opt");
    }
    if(supportSM62)
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_2");
      noResPSOs[6] =
          MakePSO()
              .RootSig(sig)
              .InputLayout(inputLayout)
              .VS(vsblob)
              .PS(Compile(common + "\n#define SM_6_2 1\n" + shaderDefines + noResourcesPixel,
                          "main", "ps_6_2", compileOptions | CompileOptionFlags::SkipOptimise))
              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      noResPSOs[6]->SetName(L"ps_6_2");
      noResPSOs[7] =
          MakePSO()
              .RootSig(sig)
              .InputLayout(inputLayout)
              .VS(vsblob)
              .PS(Compile(common + "\n#define SM_6_2 1\n" + shaderDefines + noResourcesPixel,
                          "main", "ps_6_2", compileOptions))
              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      noResPSOs[7]->SetName(L"ps_6_2_opt");
    }
    if(supportSM66)
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_6");
      noResPSOs[8] =
          MakePSO()
              .RootSig(sig)
              .InputLayout(inputLayout)
              .VS(vsblob)
              .PS(Compile(common + "\n#define SM_6_6 1\n" + shaderDefines + noResourcesPixel,
                          "main", "ps_6_6", compileOptions | CompileOptionFlags::SkipOptimise))
              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      noResPSOs[8]->SetName(L"ps_6_6");
      noResPSOs[9] =
          MakePSO()
              .RootSig(sig)
              .InputLayout(inputLayout)
              .VS(vsblob)
              .PS(Compile(common + "\n#define SM_6_6 1\n" + shaderDefines + noResourcesPixel,
                          "main", "ps_6_6", compileOptions))
              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      noResPSOs[9]->SetName(L"ps_6_6_opt");
    }

    static_assert(ARRAY_COUNT(psos) == ARRAY_COUNT(noResPSOs), "Mismatched PSO counts");

    static const uint32_t texDim = AlignUp(std::max(numResTests, numNoResTests), 64U) * 4;

    ID3D12ResourcePtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, texDim, 4)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE fltRTV = MakeRTV(fltTex).CreateCPU(0);
    D3D12_GPU_DESCRIPTOR_HANDLE fltSRV = MakeSRV(fltTex).CreateGPU(8);

    float triWidth = 8.0f / float(texDim);

    ConstsA2V triangle[] = {
        {Vec3f(-1.0f, -1.0f, triWidth), 0.0f, 1.0f, -1.0f, (float)texDim},
        {Vec3f(-1.0f, 1.0f, triWidth), 0.0f, 1.0f, -1.0f, (float)texDim},
        {Vec3f(-1.0f + triWidth, 1.0f, triWidth), 0.0f, 1.0f, -1.0f, (float)texDim},
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
    srvBuf->SetName(L"srvBuf");
    MakeSRV(srvBuf).Format(DXGI_FORMAT_R32_FLOAT).CreateGPU(0);

    int16_t test16data[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    };

    ID3D12ResourcePtr srv16Buf = MakeBuffer().Data(test16data);
    srv16Buf->SetName(L"srv16Buf");

    MakeSRV(srv16Buf)
        .Format(DXGI_FORMAT_UNKNOWN)
        .FirstElement(3)
        .NumElements(5)
        .StructureStride(2)
        .CreateGPU(42);
    MakeSRV(srv16Buf).Format(DXGI_FORMAT_R16_SINT).CreateGPU(43);

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
    rawBuf->SetName(L"rawBuf");
    MakeSRV(rawBuf)
        .Format(DXGI_FORMAT_R32_TYPELESS)
        .ByteAddressed()
        .FirstElement(4)
        .NumElements(12)
        .CreateGPU(1);

    ID3D12ResourcePtr msTex = MakeTexture(DXGI_FORMAT_R32_FLOAT, 32, 32).Multisampled(4).RTV();
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
    rawBuf2->SetName(L"rawBuf2");
    D3D12ViewCreator uavView1 =
        MakeUAV(rawBuf2).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().FirstElement(4).NumElements(24);
    D3D12_CPU_DESCRIPTOR_HANDLE uav1cpu = uavView1.CreateClearCPU(10);
    D3D12_GPU_DESCRIPTOR_HANDLE uav1gpu = uavView1.CreateGPU(10);

    uavView1 =
        MakeUAV(rawBuf2).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().FirstElement(40).NumElements(24);
    D3D12_CPU_DESCRIPTOR_HANDLE uav3cpu = uavView1.CreateClearCPU(12);
    D3D12_GPU_DESCRIPTOR_HANDLE uav3gpu = uavView1.CreateGPU(12);

    uint16_t narrowdata[32];
    for(size_t i = 0; i < ARRAY_COUNT(narrowdata); i++)
      narrowdata[i] = MakeHalf(float(i));

    ID3D12ResourcePtr narrowtypedbuf = MakeBuffer().UAV().Data(narrowdata);
    narrowtypedbuf->SetName(L"narrowtypedbuf");
    MakeSRV(narrowtypedbuf).Format(DXGI_FORMAT_R16_FLOAT).CreateGPU(22);
    MakeUAV(narrowtypedbuf).Format(DXGI_FORMAT_R16_FLOAT).CreateGPU(32);

    ID3D12ResourcePtr smileyUAV = MakeTexture(DXGI_FORMAT_R8G8B8A8_TYPELESS, 48, 48)
                                      .Mips(1)
                                      .InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                                      .UAV();

    MakeUAV(smileyUAV).Format(DXGI_FORMAT_R8G8B8A8_UNORM).CreateGPU(33);
    ID3D12ResourcePtr atomicBuffer = MakeBuffer().Size(1024).UAV();
    atomicBuffer->SetName(L"atomicBuffer");
    MakeUAV(atomicBuffer).Format(DXGI_FORMAT_R32_UINT).CreateGPU(34);
    ID3D12ResourcePtr oneIntBuffer = MakeBuffer().Size(4).UAV();
    oneIntBuffer->SetName(L"oneIntBuffer");
    MakeUAV(oneIntBuffer).Format(DXGI_FORMAT_R32_SINT).CreateGPU(100);
    ID3D12ResourcePtr typedBuffer = MakeBuffer().Size(1024).UAV();
    typedBuffer->SetName(L"typedBuffer");
    MakeUAV(typedBuffer).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateGPU(101);

    // Typed texture with UAV of UNKNOWN format
    ID3D12ResourcePtr typedTexture = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 48, 48)
                                         .Mips(1)
                                         .InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                                         .UAV();
    typedTexture->SetName(L"typedTexture");
    MakeUAV(typedTexture).Format(DXGI_FORMAT_UNKNOWN).CreateGPU(102);

    float structdata[220];
    for(int i = 0; i < 220; i++)
      structdata[i] = float(i);

    ID3D12ResourcePtr rgbbuf = MakeBuffer().Data(structdata);
    rgbbuf->SetName(L"rgbbuf");
    MakeSRV(rgbbuf).Format(DXGI_FORMAT_R32G32B32_FLOAT).CreateGPU(23);

    ID3D12ResourcePtr structBuf = MakeBuffer().Data(structdata);
    structBuf->SetName(L"structBuf");
    MakeSRV(structBuf)
        .Format(DXGI_FORMAT_UNKNOWN)
        .FirstElement(3)
        .NumElements(5)
        .StructureStride(11 * sizeof(float))
        .CreateGPU(2);

    ID3D12ResourcePtr rootStruct = MakeBuffer().Data(structdata);
    rootStruct->SetName(L"rootStruct");
    MakeSRV(rootStruct)
        .Format(DXGI_FORMAT_UNKNOWN)
        .FirstElement(3)
        .NumElements(5)
        .StructureStride(11 * sizeof(float))
        .CreateGPU(35);
    ID3D12ResourcePtr rootDummy = MakeBuffer().Data(structdata);
    rootDummy->SetName(L"rootDummy");

    ID3D12ResourcePtr structBuf2 = MakeBuffer().Size(880).UAV();
    structBuf2->SetName(L"structBuf2");
    D3D12ViewCreator uavView2 = MakeUAV(structBuf2)
                                    .Format(DXGI_FORMAT_UNKNOWN)
                                    .FirstElement(3)
                                    .NumElements(6)
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

    ID3D12PipelineStatePtr msaaPSOs[3] = {psomsaa, NULL};
    if(supportSM60)
    {
      msaaPSOs[1] = MakePSO()
                        .RootSig(sigmsaa)
                        .InputLayout()
                        .VS(Compile(D3DDefaultVertex, "main", "vs_6_0"))
                        .PS(Compile(msaaPixel, "main", "ps_6_0"))
                        .SampleCount(4)
                        .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});

      if(supportSM61)
      {
        msaaPSOs[2] = MakePSO()
                          .RootSig(sigmsaa)
                          .InputLayout()
                          .VS(Compile(D3DDefaultVertex, "main", "vs_6_1"))
                          .PS(Compile(msaaPixel61, "main", "ps_6_1"))
                          .SampleCount(4)
                          .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      }
    }

    ID3D12ResourcePtr vbmsaa = MakeBuffer().Data(DefaultTri);

    ID3D12ResourcePtr msaaTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 8, 8)
                                    .RTV()
                                    .Multisampled(4)
                                    .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE msaaRTV = MakeRTV(msaaTex).CreateCPU(1);

    ID3D12RootSignaturePtr blitSig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 1),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 8),
    });
    ID3D12PipelineStatePtr blitpso = MakePSO()
                                         .RootSig(blitSig)
                                         .VS(Compile(D3DFullscreenQuadVertex, "main", "vs_4_0"))
                                         .PS(Compile(pixelBlit, "main", "ps_5_0"));

    ID3D12RootSignaturePtr vertexSampleSig = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_VERTEX, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 8),
        },
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);

    ID3D12PipelineStatePtr vertexSamplePSO_5_0 = MakePSO()
                                                     .RootSig(vertexSampleSig)
                                                     .VS(Compile(vertexSampleVS, "main", "vs_5_0"))
                                                     .PS(Compile(vertexSamplePS, "main", "ps_5_0"));

    ID3D12PipelineStatePtr vertexSamplePSOs[3] = {vertexSamplePSO_5_0, NULL};
    if(supportSM60)
    {
      vertexSamplePSOs[1] = MakePSO()
                                .RootSig(vertexSampleSig)
                                .VS(Compile(vertexSampleVS, "main", "vs_6_0"))
                                .PS(Compile(vertexSamplePS, "main", "ps_6_0"));
    }

    if(supportSM66)
    {
      vertexSamplePSOs[2] = MakePSO()
                                .RootSig(vertexSampleSig)
                                .VS(Compile(vertexSampleVS, "main", "vs_6_6"))
                                .PS(Compile(vertexSamplePS, "main", "ps_6_6"));
    }

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

    ID3D12RootSignaturePtr bannedSig =
        MakeSig({}, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);
    ID3D12PipelineStatePtr bannedPSO = MakePSO()
                                           .InputLayout()
                                           .RootSig(bannedSig)
                                           .VS(Compile(D3DDefaultVertex, "main", "vs_5_0"))
                                           .PS(Compile(D3DDefaultPixel, "main", "ps_5_0"));

    const uint32_t renderDataSize = sizeof(float) * 22;
    // Create resources for compute shader
    const uint32_t computeDataStart = AlignUp(renderDataSize, 1024U);
    ID3D12RootSignaturePtr sigCompute = MakeSig({
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 1),
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0, 4),
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 1, 12),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2, 1, 3),
    });

    const uint32_t countComputeSMs = 3;
    ID3D12PipelineStatePtr computePSOs[countComputeSMs] = {NULL, NULL, NULL};
    std::string computeSMs[countComputeSMs] = {"cs_5_0", "cs_6_0", "cs_6_6"};
    std::string compute = shaderDefines + computeMain;
    ID3DBlobPtr csblob = Compile(compute, "main", "cs_5_0");
    computePSOs[0] = MakePSO().RootSig(sigCompute).CS(csblob);

    if(supportSM60)
    {
      csblob = Compile("#define SM_6_0 1\n" + compute, "main", "cs_6_0");
      computePSOs[1] = MakePSO().RootSig(sigCompute).CS(csblob);
    }

    if(supportSM66)
    {
      csblob = Compile("#define SM_6_6 1\n" + compute, "main", "cs_6_6", compileOptions);
      computePSOs[2] = MakePSO().RootSig(sigCompute).CS(csblob);
    }

    const uint32_t uavSize = 1024;
    ID3D12ResourcePtr bufIn = MakeBuffer().Size(uavSize).UAV();
    ID3D12ResourcePtr bufOut = MakeBuffer().Size(uavSize).UAV();
    bufIn->SetName(L"bufIn");
    bufOut->SetName(L"bufOut");

    D3D12_GPU_DESCRIPTOR_HANDLE bufInGPU =
        MakeUAV(bufIn).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateGPU(computeDataStart);
    D3D12_CPU_DESCRIPTOR_HANDLE bufInClearCPU =
        MakeUAV(bufIn).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateClearCPU(computeDataStart);
    D3D12_GPU_DESCRIPTOR_HANDLE bufOutGPU =
        MakeUAV(bufOut).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateGPU(computeDataStart + 1);
    D3D12_CPU_DESCRIPTOR_HANDLE bufOutClearCPU =
        MakeUAV(bufOut).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateClearCPU(computeDataStart + 1);

    D3D12_GPU_VIRTUAL_ADDRESS bufInVA = bufIn->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS bufOutVA = bufOut->GetGPUVirtualAddress();

    uint32_t bufInInitData[uavSize];
    uint32_t bufOutInitData[uavSize];
    for(uint32_t i = 0; i < uavSize; ++i)
    {
      bufInInitData[i] = 111 + i / 4;
      bufOutInitData[i] = 222 + i / 4;
    }

    D3D12_RECT uavClearRect = {};
    uavClearRect.right = uavSize;
    uavClearRect.bottom = 1;

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(2);
      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      setMarker(cmd, undefined_tests);

      const char *markers[] = {
          "sm_5_0",     "sm_5_0_opt", "sm_5_1",     "sm_5_1_opt", "sm_6_0",
          "sm_6_0_opt", "sm_6_2",     "sm_6_2_opt", "sm_6_6",     "sm_6_6_opt",
      };
      static_assert(ARRAY_COUNT(markers) == ARRAY_COUNT(psos), "mismatched array dimension");

      // Clear, draw, and blit to backbuffer - once for each SM 5.0, 5.1, 6.0, 6.2, 6.6
      size_t countGraphicsPasses = 4;
      if(supportSM60)
        countGraphicsPasses += 2;
      if(supportSM62)
        countGraphicsPasses += 2;
      if(supportSM66)
        countGraphicsPasses += 2;
      TEST_ASSERT(countGraphicsPasses <= ARRAY_COUNT(psos), "More graphic passes than psos");
      for(size_t i = 0; i < countGraphicsPasses; ++i)
      {
        float blitOffset = 8.0f * i;
        D3D12_RECT scissor = {};
        scissor.left = 0;
        scissor.top = (int)(8 * i);
        scissor.right = (int)texDim;

        for(size_t j = 0; j < 2; ++j)
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
              6, rootStruct->GetGPUVirtualAddress() + renderDataSize);
          cmd->SetGraphicsRootDescriptorTable(7, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

          // Add a marker so we can easily locate this draw
          std::string markerName = markers[i];
          uint32_t numTests = 0;
          ID3D12PipelineStatePtr pso = NULL;
          if(j == 0)
          {
            pso = psos[i];
            numTests = numResTests;
          }
          else
          {
            markerName = "NoResources " + markerName;
            pso = noResPSOs[i];
            numTests = numNoResTests;
          }
          cmd->SetPipelineState(pso);

          RSSetViewport(cmd, {0.0f, 0.0f, (float)texDim, 4.0f, 0.0f, 1.0f});
          RSSetScissorRect(cmd, {0, 0, (int)texDim, 4});

          UINT zero[4] = {};
          cmd->ClearUnorderedAccessViewUint(uav1gpu, uav1cpu, rawBuf2, zero, 0, NULL);
          cmd->ClearUnorderedAccessViewUint(uav2gpu, uav2cpu, structBuf2, zero, 0, NULL);
          cmd->ClearUnorderedAccessViewUint(uav3gpu, uav3cpu, rawBuf2, zero, 0, NULL);

          setMarker(cmd, markerName.c_str());
          cmd->DrawInstanced(3, numTests, 0, 0);

          ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

          scissor.bottom = scissor.top + 4;
          OMSetRenderTargets(cmd, {rtv}, {});
          RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
          RSSetScissorRect(cmd, scissor);

          cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
          cmd->SetGraphicsRootSignature(blitSig);
          cmd->SetPipelineState(blitpso);
          cmd->SetGraphicsRoot32BitConstant(0, *(UINT *)&blitOffset, 0);
          cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
          cmd->DrawInstanced(4, 1, 0, 0);

          ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                          D3D12_RESOURCE_STATE_RENDER_TARGET);

          scissor.top += 4;
          blitOffset += 4.0f;
        }
      }

      // Render MSAA test
      OMSetRenderTargets(cmd, {msaaRTV}, {});
      ClearRenderTargetView(cmd, msaaRTV, {0.2f, 0.2f, 0.2f, 1.0f});
      IASetVertexBuffer(cmd, vbmsaa, sizeof(DefaultA2V), 0);
      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      RSSetViewport(cmd, {0.0f, 0.0f, 8.0f, 8.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, 8, 8});

      size_t countMSAAPasses = supportSM60 ? (supportSM61 ? 3 : 2) : 1;
      TEST_ASSERT(countMSAAPasses <= ARRAY_COUNT(msaaPSOs), "More MSAA passes than psos");
      const char *msaa_markers[3] = {
          "MSAA sm_5_0",
          "MSAA sm_6_0",
          "MSAA sm_6_1",
      };
      for(int i = 0; i < countMSAAPasses; ++i)
      {
        cmd->SetGraphicsRootSignature(sigmsaa);
        cmd->SetPipelineState(msaaPSOs[i]);

        // Add a marker so we can easily locate this draw
        setMarker(cmd, msaa_markers[i]);
        cmd->DrawInstanced(3, 1, 0, 0);
      }

      OMSetRenderTargets(cmd, {fltRTV}, {});
      ClearRenderTargetView(cmd, fltRTV, {0.3f, 0.5f, 0.8f, 1.0f});

      ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

      OMSetRenderTargets(cmd, {rtv}, {});
      RSSetViewport(cmd, {50.0f, 50.0f, 10.0f, 10.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {50, 50, 60, 60});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      cmd->SetGraphicsRootSignature(vertexSampleSig);

      const char *vs_markers[3] = {
          "VertexSample sm_5_0",
          "VertexSample sm_6_0",
          "VertexSample sm_6_6",
      };
      size_t countVertexSamplePasses = supportSM66 ? 3 : (supportSM60 ? 2 : 1);
      TEST_ASSERT(countVertexSamplePasses <= ARRAY_COUNT(vertexSamplePSOs),
                  "More vertex sample passes than psos");
      for(int i = 0; i < countVertexSamplePasses; ++i)
      {
        cmd->SetPipelineState(vertexSamplePSOs[i]);
        cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        setMarker(cmd, vs_markers[i]);
        cmd->DrawInstanced(4, 1, 0, 0);
      }

      setMarker(cmd, "BannedSig");
      RSSetViewport(cmd, {60.0f, 60.0f, 10.0f, 10.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {60, 60, 70, 70});
      cmd->SetGraphicsRootSignature(bannedSig);
      cmd->SetPipelineState(bannedPSO);
      cmd->DrawInstanced(3, 1, 0, 0);

      ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      pushMarker(cmd, "Compute");
      size_t countComputePasses = supportSM66 ? 3 : (supportSM60 ? 2 : 1);
      TEST_ASSERT(countComputePasses <= ARRAY_COUNT(computePSOs), "More compute passes than psos");
      for(size_t i = 0; i < countComputePasses; ++i)
      {
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

        cmd->ClearUnorderedAccessViewUint(bufInGPU, bufInClearCPU, bufIn, bufInInitData, 1,
                                          &uavClearRect);
        cmd->ClearUnorderedAccessViewUint(bufOutGPU, bufOutClearCPU, bufOut, bufOutInitData, 1,
                                          &uavClearRect);

        cmd->SetComputeRootSignature(sigCompute);
        cmd->SetComputeRootUnorderedAccessView(0, bufInVA);
        cmd->SetComputeRootUnorderedAccessView(1, bufOutVA);
        cmd->SetComputeRoot32BitConstant(2, 5, 0);
        cmd->SetComputeRoot32BitConstant(2, 6, 1);
        cmd->SetComputeRoot32BitConstant(2, 7, 2);
        cmd->SetComputeRoot32BitConstant(2, 8, 3);
        cmd->SetComputeRoot32BitConstant(3, 10, 4 + 2);    // col1z
        cmd->SetComputeRoot32BitConstant(3, 11, 8 + 3);    // col2w
        cmd->SetComputeRootDescriptorTable(4, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

        cmd->SetPipelineState(computePSOs[i]);
        setMarker(cmd, computeSMs[i]);
        cmd->Dispatch(numComputeTests, 2, 1);
      }
      popMarker(cmd);

      cmd->Close();
      Submit({cmd});
      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
