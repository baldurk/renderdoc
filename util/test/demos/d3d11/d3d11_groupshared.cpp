/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
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

RD_TEST(D3D11_Groupshared, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Test of compute shader that uses groupshared memory.";

  std::string comp = R"EOSHADER(

#define MAX_THREADS 64

RWStructuredBuffer<float> indata : register(u0);
RWStructuredBuffer<float4> outdata : register(u1);

groupshared float gsmData[MAX_THREADS];
groupshared float gsmSmallArray[MAX_THREADS];
groupshared float3 gsmSmall2DArray[3][MAX_THREADS];

cbuffer consts : register(b0)
{
  int inTest;
};

#define IsTest(x) (inTest == x)

float GetGSMValue(uint i)
{
  return gsmData[i % MAX_THREADS];
}

[numthreads(MAX_THREADS,1,1)]
void main(uint3 gid : SV_GroupThreadID)
{
  if(gid.x == 0)
  {
    for(int i=0; i < MAX_THREADS; i++) gsmData[i] = 1.25f;
  }

  GroupMemoryBarrierWithGroupSync();

  float4 outval = 0.0f.xxxx;

  if (IsTest(0))
  {
    // first write, should be the init value for all threads
    outval.x = GetGSMValue(gid.x);

    gsmData[gid.x] = indata[gid.x];

    // second write, should be the read value because we're reading our own value
    outval.y = GetGSMValue(gid.x);

    GroupMemoryBarrierWithGroupSync();

    // third write, should be our pairwise neighbour's value
    outval.z = GetGSMValue(gid.x ^ 1);

    // do calculation with our neighbour
    gsmData[gid.x] = (1.0f + GetGSMValue(gid.x)) * (1.0f + GetGSMValue(gid.x ^ 1));

    GroupMemoryBarrierWithGroupSync();

    // fourth write, our neighbour should be identical to our value
    outval.w = GetGSMValue(gid.x) == GetGSMValue(gid.x ^ 1) ? 9.99f : -9.99f;
  }
  else if (IsTest(1))
  {
    gsmData[gid.x] = (float)gid.x;
    gsmData[gid.x] += 10.0f;
    GroupMemoryBarrierWithGroupSync();

    outval.x = GetGSMValue(gid.x);
    outval.y = GetGSMValue(gid.x + 1);

    GroupMemoryBarrierWithGroupSync();
    gsmData[gid.x] += 10.0f;
    GroupMemoryBarrierWithGroupSync();

    outval.z = GetGSMValue(gid.x + 2);

    GroupMemoryBarrierWithGroupSync();
    gsmData[gid.x] += 10.0f;
    GroupMemoryBarrierWithGroupSync();

    outval.w = GetGSMValue(gid.x + 3);
  }
  else if (IsTest(2))
  {
    // Deliberately no sync to test debugger behaviour not GPU correctness
    // Debugger should see the initial value of 1.25f for all of GSM
    gsmData[gid.x] = (float)gid.x;
    outval.x = GetGSMValue(gid.x);
    outval.y = GetGSMValue(gid.x + 1);
    outval.z = GetGSMValue(gid.x + 2);
    outval.w = GetGSMValue(gid.x + 3);
  }
  else if (IsTest(3))
  {
    // Out of bounds GSM access should not crash
    uint idx = gid.x;
    uint oneu = (idx+1)/(idx+1);
    uint zerou = oneu - 1;
    uint idxOOB = idx * 2 + MAX_THREADS * oneu;

    gsmSmallArray[idx] = (float)gid.x;
    gsmSmall2DArray[zerou][idx].x = (float)gid.x;
    gsmSmall2DArray[zerou][idx].y = (float)gid.x + 1.0;
    gsmSmall2DArray[oneu][idx].x = (float)gid.x;
    gsmSmall2DArray[oneu][idx].y = (float)gid.x + 1.0;
    gsmSmall2DArray[oneu+oneu][idx].x = (float)gid.x;
    gsmSmall2DArray[oneu+oneu][idx].y = (float)gid.x + 1.0;

    gsmSmallArray[idxOOB] += (float)zerou;
    gsmSmall2DArray[oneu][idxOOB].x += (float)zerou;
    gsmSmall2DArray[oneu][idxOOB].y += (float)zerou;

    outval.x = gsmSmallArray[idx + zerou];
    outval.y = gsmSmallArray[idx + zerou];
    outval.z = gsmSmall2DArray[oneu][idx + zerou].x;
    outval.w = gsmSmall2DArray[oneu][idx + zerou].y;
  }

  outdata[gid.x] = outval;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    float values[64];
    for(int i = 0; i < 64; i++)
      values[i] = RANDF(1.0f, 100.0f);
    ID3D11BufferPtr inBuf = MakeBuffer().Data(values).UAV().Structured(4);
    ID3D11BufferPtr outBuf = MakeBuffer().Size(sizeof(Vec4f) * 64).UAV().Structured(sizeof(Vec4f));

    ID3D11UnorderedAccessViewPtr inUAV = MakeUAV(inBuf);
    ID3D11UnorderedAccessViewPtr outUAV = MakeUAV(outBuf);

    ID3D11ComputeShaderPtr shad = CreateCS(Compile(comp, "main", "cs_5_0"));

    int cbufferdata[4];
    memset(cbufferdata, 0, sizeof(cbufferdata));
    ID3D11BufferPtr cb = MakeBuffer().Size(16).Constant().Data(&cbufferdata);

    int numCompTests = 0;
    size_t pos = 0;
    while(pos != std::string::npos)
    {
      pos = comp.find("IsTest(", pos);
      if(pos == std::string::npos)
        break;
      pos += sizeof("IsTest(") - 1;
      numCompTests = std::max(numCompTests, atoi(comp.c_str() + pos) + 1);
    }

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      ctx->CSSetShader(shad, NULL, 0);
      ctx->CSSetUnorderedAccessViews(0, 1, &inUAV.GetInterfacePtr(), NULL);
      ctx->CSSetUnorderedAccessViews(1, 1, &outUAV.GetInterfacePtr(), NULL);

      pushMarker("Compute Tests");
      for(int i = 0; i < numCompTests; ++i)
      {
        ClearUnorderedAccessView(outUAV, Vec4u());
        ctx->UpdateSubresource(cb, 0, NULL, &i, 4, 0);
        ctx->CSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());
        ctx->Dispatch(1, 1, 1);
      }
      popMarker();

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
