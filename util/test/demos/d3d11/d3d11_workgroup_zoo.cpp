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

#include "3rdparty/fmt/core.h"
#include "d3d11_test.h"

RD_TEST(D3D11_Workgroup_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test of behaviour around workgroup operations in shaders.";

  const std::string common = R"EOSHADER(

cbuffer rootconsts : register(b0)
{
  uint root_test;
  uint root_two;
}

uint GetTest() { return root_test; }

#define IsTest(x) (GetTest() == x)

)EOSHADER";

  const std::string compCommon = common + R"EOSHADER(

RWStructuredBuffer<float4> outbuf : register(u0);

static uint3 tid;
static uint flatId;

groupshared uint4 gsmUint4[1024];

void SetOutput(float4 val)
{
  outbuf[GetTest() * 1024 + flatId] = val;
}

void Init(float4 val)
{
  flatId = tid.x + GROUP_SIZE_X * tid.y + GROUP_SIZE_X * GROUP_SIZE_Y * tid.z;
  gsmUint4[flatId].xyz = tid;
  gsmUint4[flatId].z = tid.x;
  SetOutput(val);
}

)EOSHADER";

  const std::string testShader = compCommon + R"EOSHADER(

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 inGTid : SV_GroupThreadID)
{
  tid = inGTid;
  float4 testResult = 0.0f.xxxx;
  Init(testResult);
  uint id = flatId;

  if(IsTest(0))
  {
    testResult.x = id;
    AllMemoryBarrierWithGroupSync();
  }

  SetOutput(testResult);
}

)EOSHADER";

  const std::string perfShader = compCommon + R"EOSHADER(

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, GROUP_SIZE_Z)]
void main(uint3 inGTid : SV_GroupThreadID)
{
  tid = inGTid;
  float4 testResult = 0.0f.xxxx;
  Init(testResult);
  uint id = flatId;

  // TEST CASES:
  // 0: GPU math : loops 100
  // 1: CPU math : loops 100
  // 2: GPU math : loops 200
  // 3: CPU math : loops 200
  // 4: GPU math : loops 400
  // 5: CPU math : loops 400
  // 6: GPU math : loops 5000
  // 7: CPU math : loops 5000
  bool useCpu = GetTest() & 0x1;

  uint count = 0;
  {
    uint temp = GetTest() >> 1;
    if(temp == 0)
      count = 100U; 
    if(temp == 1)
      count = 200U; 
    if(temp == 2)
      count = 400U; 
    if(temp == 3)
      count = 5000U; 
  }

  if(useCpu)
  {
    for (uint i = 0; i < count; ++i)
    {
       gsmUint4[id].x += i;
       gsmUint4[id].y += i * i;
       testResult.x = testResult.x * testResult.x;
       testResult.x += dot(gsmUint4[id], gsmUint4[id]);
    }
  }
  else
  {
    for (uint i = 0; i < count; ++i)
    {
       gsmUint4[id].x += i;
       gsmUint4[id].y += i * i;
       testResult.x = pow(testResult.x, float(root_two));
       testResult.x += dot(gsmUint4[id], gsmUint4[id]);
    }
  }

  AllMemoryBarrierWithGroupSync();

  SetOutput(testResult);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11BufferPtr outBuf = MakeBuffer().Size(sizeof(Vec4f) * 1024).UAV().Structured(sizeof(Vec4f));
    ID3D11UnorderedAccessViewPtr outUAV = MakeUAV(outBuf);

    int cbufferdata[4];
    memset(cbufferdata, 0, sizeof(cbufferdata));
    ID3D11BufferPtr cb = MakeBuffer().Size(16).Constant().Data(&cbufferdata);

    int32_t countCompTests = 0;
    size_t pos = 0;
    while(pos != std::string::npos)
    {
      pos = testShader.find("IsTest(", pos);
      if(pos == std::string::npos)
        break;
      pos += sizeof("IsTest(") - 1;
      countCompTests = std::max(countCompTests, atoi(testShader.c_str() + pos) + 1);
    }

    const int32_t countPerfTests = 8;

    struct CompSize
    {
      int x, y, z;
    };
    CompSize compsizes[] = {
        {70, 1, 1},
    };
    std::string comppipe_name[ARRAY_COUNT(compsizes)];
    ID3D11ComputeShaderPtr testShaders[ARRAY_COUNT(compsizes)];
    ID3D11ComputeShaderPtr perfShaders[ARRAY_COUNT(compsizes)];

    std::string defines;

    for(int i = 0; i < ARRAY_COUNT(compsizes); i++)
    {
      std::string sizedefine;
      sizedefine =
          fmt::format("#define GROUP_SIZE_X {}\n#define GROUP_SIZE_Y {}\n#define GROUP_SIZE_Z {}",
                      compsizes[i].x, compsizes[i].y, compsizes[i].z);
      comppipe_name[i] = fmt::format("{}x{}x{}", compsizes[i].x, compsizes[i].y, compsizes[i].z);

      testShaders[i] = CreateCS(Compile(defines + sizedefine + testShader, "main", "cs_5_0"));
      perfShaders[i] = CreateCS(Compile(defines + sizedefine + perfShader, "main", "cs_5_0"));
    }

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      pushMarker("Compute Tests");
      for(size_t p = 0; p < ARRAY_COUNT(compsizes); p++)
      {
        pushMarker(comppipe_name[p]);
        ctx->CSSetShader(testShaders[p], NULL, 0);
        ctx->CSSetUnorderedAccessViews(0, 1, &outUAV.GetInterfacePtr(), NULL);

        for(int i = 0; i < countCompTests; ++i)
        {
          ClearUnorderedAccessView(outUAV, Vec4u());
          ctx->UpdateSubresource(cb, 0, NULL, &i, 8, 0);
          ctx->CSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());
          ctx->Dispatch(2, 1, 1);
          popMarker();
        }
        popMarker();
      }

      pushMarker("Perf Tests");
      for(size_t p = 0; p < ARRAY_COUNT(compsizes); p++)
      {
        pushMarker(comppipe_name[p]);
        ctx->CSSetShader(perfShaders[p], NULL, 0);
        ctx->CSSetUnorderedAccessViews(0, 1, &outUAV.GetInterfacePtr(), NULL);

        for(int i = 0; i < countPerfTests; ++i)
        {
          cbufferdata[0] = i;
          cbufferdata[1] = 2;

          bool useCpu = (i & 0x1);
          int count = 0;
          {
            int temp = i >> 1;
            if(temp == 0)
              count = 100U;
            if(temp == 1)
              count = 200U;
            if(temp == 2)
              count = 400U;
            if(temp == 3)
              count = 5000U;
          }
          std::string perfTestName =
              fmt::format("{} Iterations {} Math", count, useCpu ? "CPU" : "GPU");
          pushMarker(perfTestName);
          ClearUnorderedAccessView(outUAV, Vec4u());
          ctx->UpdateSubresource(cb, 0, NULL, cbufferdata, 8, 0);
          ctx->CSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());
          ctx->Dispatch(2, 1, 1);
          popMarker();
        }
        popMarker();
      }
      popMarker();

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
