/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Baldur Karlsson
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

RD_TEST(D3D12_Shader_DebugData_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests shader debug data from different sources";

  struct ConstsA2V
  {
    Vec3f pos;
  };

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
};

struct v2f
{
  float4 pos : SV_POSITION;
  uint tri : TRIANGLE;
  uint intval : INTVAL;
  row_major float3x4 mat : MAT;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

v2f main(consts IN, uint tri : SV_InstanceID)
{
  v2f OUT = (v2f)0;

  OUT.pos = float4(IN.pos.x + IN.pos.z * float(tri), IN.pos.y, 0.0f, 1);

  OUT.tri = tri;
  OUT.intval = tri + 11;

  OUT.mat._m00 = 1.0;
  OUT.mat._m01 = 2.0;
  OUT.mat._m02 = 3.0;
  OUT.mat._m03 = 4.0;
  OUT.mat._m10 = 5.0;
  OUT.mat._m11 = 6.0;
  OUT.mat._m12 = 7.0;
  OUT.mat._m13 = 8.0;
  OUT.mat._m20 = 9.0;
  OUT.mat._m21 = 10.0;
  OUT.mat._m22 = 11.0;
  OUT.mat._m23 = 12.0;

  return OUT;
}

)EOSHADER";

  std::string testDefines = R"EOSHADER(

#define TEST_RESULT 0
#define TEST_DEBUG_TYPE(TYPE) TYPE __test_ ## TYPE = 0;
#define TEST_DEBUG_VECTOR_TYPE(TYPE) \
TEST_DEBUG_TYPE(TYPE) \
TEST_DEBUG_TYPE(TYPE ## 1) \
TEST_DEBUG_TYPE(TYPE ## 2) \
TEST_DEBUG_TYPE(TYPE ## 3) \
TEST_DEBUG_TYPE(TYPE ## 4) 
#define TEST_DEBUG_MATRIX23_TYPE(TYPE) \
row_major TYPE ## 2x3 __test_ ## TYPE ## 23  = 0;

#define USE_DEBUG_VECTOR_TYPE(TYPE) \
  testResult.x += __test_ ## TYPE .x; \
  testResult.x += __test_ ## TYPE ## 1 .x; \
  testResult.xy += __test_ ## TYPE ## 2 .xy; \
  testResult.xyz += __test_ ## TYPE ## 3 .xyz; \
  testResult.xyzw += __test_ ## TYPE ## 4 .xyzw; 

#define USE_DEBUG_MATRIX23_TYPE(TYPE) \
  testResult.xyz += __test_ ## TYPE ## 23[0] .xyz; \
  testResult.xyz += __test_ ## TYPE ## 23[1] .xyz; \

#define TEST_DEBUG_VAR_SET(TYPE, VAR, VALUE) \
  VAR = VALUE * __ONE; \
  __test_ ## TYPE += VAR;

#define TEST_DEBUG_VAR_DECLARE(TYPE, VAR, VALUE) TYPE VAR; TEST_DEBUG_VAR_SET(TYPE, VAR, VALUE)

#define TEST_DEBUG_VAR_DECLARE_MATRIX23(TYPE, VAR, VALUE) \
  row_major TYPE ## 2x3 VAR; \
  VAR = VALUE * __ONE; \
  __test_ ## TYPE ## 23 = VAR; \
  __test_ ## TYPE ## 23 ._m00 += VALUE * __ONE; \
  __test_ ## TYPE ## 23 ._m01 += VALUE * __TWO; \
  __test_ ## TYPE ## 23 ._m02 += VALUE * __THREE; \
  __test_ ## TYPE ## 23 ._m10 += VALUE * __FOUR; \
  __test_ ## TYPE ## 23 ._m11 += VALUE * __FIVE; \
  __test_ ## TYPE ## 23 ._m12 += VALUE * __SIX; \

struct TestStruct
{
  struct
  {
    int4 a;
    int3 b;
  } anon;
};

struct TestGrand
{
  int3 i3;
  int2 i2;
};

struct TestParent : TestGrand
{
  int4 i4;
};

struct TestChild : TestParent
{
  int i;
};

)EOSHADER";

  std::string testsBody = R"EOSHADER(

  TestStruct testStruct;
  TestChild testChild;

  TEST_DEBUG_VECTOR_TYPE(int);
  TEST_DEBUG_VECTOR_TYPE(float);
  TEST_DEBUG_MATRIX23_TYPE(float);

  // TEST_DEBUG_VAR_START
  TEST_DEBUG_VAR_DECLARE(int, testIndex, TEST_INDEX)
  TEST_DEBUG_VAR_DECLARE(int1, intVal, TEST_INDEX)
  TEST_DEBUG_VAR_DECLARE(float4, testResult, TEST_RESULT)
  TEST_DEBUG_VAR_DECLARE(int2, jake, 5)
  TEST_DEBUG_VAR_DECLARE(float1, bob, 3.0)
  TEST_DEBUG_VAR_SET(int4, testStruct.anon.a, 1)
  TEST_DEBUG_VAR_SET(int3, testStruct.anon.b, 2)
  TEST_DEBUG_VAR_SET(int, testChild.i, 1)
  TEST_DEBUG_VAR_SET(int2, testChild.i2, 2)
  TEST_DEBUG_VAR_SET(int3, testChild.i3, 3)
  TEST_DEBUG_VAR_SET(int4, testChild.i4, 4)
  TEST_DEBUG_VAR_DECLARE_MATRIX23(float, fish, 7.0)
  // TEST_DEBUG_VAR_END

  if(testIndex == 0)
  {
    testResult.x = intVal * 7;
    testResult.y = intVal * 5;
    SET_DATA(testIndex, testResult);
    EARLY_RETURN;
  }
  else if(testIndex == 1)
  {
    testResult = intVal;
  }
  else if(testIndex == 2)
  {
    testResult = intVal * 2;
  }
  else if(testIndex == 3)
  {
    testResult = intVal / 2;
  }
  else if(testIndex == 4)
  {
    testResult.x = testStruct.anon.a.x;
    testResult.y = testStruct.anon.b.x;
  }
  else if(testIndex == 5)
  {
    testResult.x = testChild.i;
    testResult.y = testChild.i2.x;
    testResult.z = testChild.i3.y;
    testResult.w = testChild.i4.z;
  }
  else if(testIndex == 6)
  {
    testResult.x = fish[0].x + fish[1].x;
    testResult.y = fish[0].y + fish[1].y;
    testResult.z = fish[0].z + fish[1].z;
    testResult.w = 0.0;
  }
  else
  {
    testResult = 0.4f;
  }

  USE_DEBUG_VECTOR_TYPE(int);
  USE_DEBUG_VECTOR_TYPE(float);
  USE_DEBUG_MATRIX23_TYPE(float);
)EOSHADER";

  std::string pixel = R"EOSHADER(

#define SET_DATA(INDEX, DATA)
#define EARLY_RETURN return testResult
#define TEST_INDEX IN.tri
)EOSHADER" + testDefines +
                      R"EOSHADER(

float4 main(v2f IN) : SV_Target0
{
  int __ONE = floor((IN.intval - 11)/(IN.tri+1.0e-6f)) + 1;
  __ONE += floor(abs(IN.pos.x / 1.0e6f));
  __ONE += floor(abs(IN.pos.y / 1.0e6f));
  __ONE += floor(abs(IN.pos.z / 1.0e6f));
  __ONE += floor(abs(IN.pos.w / 1.0e6f));
  for (int i = 0; i < 3; i++)
  {
    __ONE += floor(abs(IN.mat[i].x / 1.0e6f));
    __ONE += floor(abs(IN.mat[i].y / 1.0e6f));
    __ONE += floor(abs(IN.mat[i].z / 1.0e6f));
    __ONE += floor(abs(IN.mat[i].w / 1.0e6f));
  }
  int __TWO = __ONE + 1;
  int __THREE = __TWO + 1;
  int __FOUR = __THREE + 1;
  int __FIVE = __FOUR + 1;
  int __SIX = __FIVE + 1;

)EOSHADER" + testsBody +
                      R"EOSHADER(

  return testResult;
}

)EOSHADER";

  std::string compute = R"EOSHADER(

RWStructuredBuffer<float4> bufOut : register(u0);

void SetOutput(uint index, float4 data)
{
  bufOut[index] = data;
}

#define SET_DATA(INDEX, DATA) SetOutput(INDEX, DATA)
#define EARLY_RETURN return
#define TEST_INDEX inTestIndex
)EOSHADER" + testDefines +
                        R"EOSHADER(

[numthreads(1,1,1)]
void main(int inTestIndex: SV_GroupID)
{
  int __ONE = floor(inTestIndex/(inTestIndex+1.0e-6f)) + 1;
  int __TWO = __ONE + 1;
  int __THREE = __TWO + 1;
  int __FOUR = __THREE + 1;
  int __FIVE = __FOUR + 1;
  int __SIX = __FIVE + 1;
)EOSHADER" + testsBody +
                        R"EOSHADER(

  SetOutput(testIndex, testResult);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    bool supportSM60 = (m_HighestShaderModel >= D3D_SHADER_MODEL_6_0) && m_DXILSupport;
    if(!supportSM60)
      Avail = "Shader Model 6.0 and DXIL is not supported";
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    const char *testMatch = NULL;
    size_t lastTest = 0;

    testMatch = "testIndex == ";
    lastTest = pixel.rfind(testMatch);
    lastTest += strlen(testMatch);
    const uint32_t numPSTests = atoi(pixel.c_str() + lastTest) + 1;

    lastTest = compute.rfind(testMatch);
    lastTest += strlen(testMatch);
    const uint32_t numCSTests = atoi(compute.c_str() + lastTest) + 1;

    TEST_ASSERT(numPSTests == numCSTests,
                "Mismatched number of tests between pixel and compute shaders");

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

    D3D12_STATIC_SAMPLER_DESC staticSamp = {};
    staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamp.AddressU = staticSamp.AddressV = staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    ID3D12RootSignaturePtr sig =
        MakeSig({}, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);

    const int numShaderModels = 1;    // 6.0
    ID3D12PipelineStatePtr psos[numShaderModels] = {};

    uint32_t compileOptions = CompileOptionFlags::None;
    std::string shaderDefines = "";
    {
      ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_6_0");
      psos[0] = MakePSO()
                    .RootSig(sig)
                    .InputLayout(inputLayout)
                    .VS(vsblob)
                    .PS(Compile(common + "\n#define SM_6_0 1\n" + shaderDefines + pixel, "main",
                                "ps_6_0", compileOptions | CompileOptionFlags::SkipOptimise))
                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
      psos[0]->SetName(L"ps_6_0");
    }
    static const uint32_t texDim = AlignUp(numPSTests, 64U) * 4;

    ID3D12ResourcePtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, texDim, 4)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE fltRTV = MakeRTV(fltTex).CreateCPU(0);
    D3D12_GPU_DESCRIPTOR_HANDLE fltSRV = MakeSRV(fltTex).CreateGPU(8);

    float triWidth = 8.0f / float(texDim);

    ConstsA2V triangle[] = {
        {Vec3f(-1.0f, -1.0f, triWidth)},
        {Vec3f(-1.0f, 1.0f, triWidth)},
        {Vec3f(-1.0f + triWidth, 1.0f, triWidth)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(triangle);
    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12RootSignaturePtr blitSig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 1),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 8),
    });
    ID3D12PipelineStatePtr blitpso = MakePSO()
                                         .RootSig(blitSig)
                                         .VS(Compile(D3DFullscreenQuadVertex, "main", "vs_4_0"))
                                         .PS(Compile(pixelBlit, "main", "ps_5_0"));

    const uint32_t renderDataSize = sizeof(float) * 22;
    // Create resources for compute shader
    const uint32_t computeDataStart = AlignUp(renderDataSize, 1024U);
    ID3D12RootSignaturePtr sigCompute = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 2),
    });

    const uint32_t countComputeSMs = 1;
    ID3D12PipelineStatePtr computePSOs[countComputeSMs] = {NULL};
    std::string computeSMs[countComputeSMs] = {"cs_6_0"};
    ID3DBlobPtr csblob =
        Compile(compute, "main", "cs_6_0", compileOptions | CompileOptionFlags::SkipOptimise);
    computePSOs[0] = MakePSO().RootSig(sigCompute).CS(csblob);

    const uint32_t uavSize = AlignUp(numCSTests, 1024U);
    ID3D12ResourcePtr bufOut = MakeBuffer().Size(uavSize).UAV();
    bufOut->SetName(L"bufOut");

    D3D12_GPU_DESCRIPTOR_HANDLE bufOutGPU =
        MakeUAV(bufOut).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateGPU(computeDataStart + 0);
    D3D12_CPU_DESCRIPTOR_HANDLE bufOutClearCPU =
        MakeUAV(bufOut).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateClearCPU(computeDataStart + 0);

    {
      D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 0;

      D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
      uavdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      uavdesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uavdesc.Buffer.NumElements = numCSTests;

      dev->CreateUnorderedAccessView(bufOut, NULL, &uavdesc, cpu);
    }

    std::vector<float> bufOutInitData;
    bufOutInitData.resize(uavSize);
    for(uint32_t i = 0; i < uavSize; ++i)
    {
      bufOutInitData[i] = 222.0f + i / 4.0f;
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

      float blitOffsets[] = {0.0f};
      D3D12_RECT scissors[] = {
          {0, 0, (int)texDim, 4},
      };
      const char *markers[] = {
          "sm_6_0",
      };
      static_assert(ARRAY_COUNT(blitOffsets) == ARRAY_COUNT(psos), "mismatched array dimension");
      static_assert(ARRAY_COUNT(scissors) == ARRAY_COUNT(psos), "mismatched array dimension");
      static_assert(ARRAY_COUNT(markers) == ARRAY_COUNT(psos), "mismatched array dimension");

      // Clear, draw, and blit to backbuffer
      size_t countGraphicsPasses = 1;
      TEST_ASSERT(countGraphicsPasses <= ARRAY_COUNT(psos), "More graphic passes than psos");
      pushMarker(cmd, "Pixel");
      for(size_t i = 0; i < countGraphicsPasses; ++i)
      {
        OMSetRenderTargets(cmd, {fltRTV}, {});
        ClearRenderTargetView(cmd, fltRTV, {0.2f, 0.2f, 0.2f, 1.0f});

        IASetVertexBuffer(cmd, vb, sizeof(ConstsA2V), 0);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmd->SetGraphicsRootSignature(sig);
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        cmd->SetPipelineState(psos[i]);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)texDim, 4.0f, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, (int)texDim, 4});

        // Add a marker so we can easily locate this draw
        setMarker(cmd, markers[i]);
        cmd->DrawInstanced(3, numPSTests, 0, 0);

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

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
      popMarker(cmd);

      pushMarker(cmd, "Compute");
      size_t countComputePasses = 1;
      TEST_ASSERT(countComputePasses <= ARRAY_COUNT(computePSOs), "More compute passes than psos");
      for(size_t i = 0; i < countComputePasses; ++i)
      {
        cmd->ClearUnorderedAccessViewFloat(bufOutGPU, bufOutClearCPU, bufOut, bufOutInitData.data(),
                                           1, &uavClearRect);

        cmd->SetComputeRootSignature(sigCompute);
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

        cmd->SetPipelineState(computePSOs[i]);
        setMarker(cmd, computeSMs[i]);
        cmd->Dispatch(numCSTests, 1, 1);
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
