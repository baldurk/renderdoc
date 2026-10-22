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

#include "d3d12_test.h"

RD_TEST(D3D12_Groupshared, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Test of compute shader that uses groupshared memory.";

  std::string comp = R"EOSHADER(

#define MAX_THREADS 64

RWStructuredBuffer<float> indata : register(u0);
RWStructuredBuffer<float4> outdata : register(u1);

cbuffer rootconsts : register(b0)
{
  uint root_test;
}

groupshared float gsmData[MAX_THREADS];
groupshared int gsmIntData[MAX_THREADS];
groupshared int gInt;
groupshared float gsmSmallArray[MAX_THREADS];
groupshared float3 gsmSmall2DArray[3][MAX_THREADS];

#define IsTest(x) (root_test == x)

float GetGSMValue(uint i)
{
  return gsmData[i % MAX_THREADS];
}

int GetGSMIntValue(uint i)
{
  return gsmIntData[i % MAX_THREADS];
}

[numthreads(MAX_THREADS,1,1)]
void main(uint3 gid : SV_GroupThreadID)
{
  if(gid.x == 0)
  {
    gsmData[0] = 1.25f;
    gsmData[1] = 1.25f;
    gsmData[2] = 1.25f;
    gsmData[3] = 1.25f;
    gsmData[4] = 1.25f;
    gsmData[5] = 1.25f;
    gsmData[6] = 1.25f;
    gsmData[7] = 1.25f;
    gsmData[8] = 1.25f;
    for(int i=8; i < MAX_THREADS; i++) gsmData[i] = 1.25f;
    gsmIntData[0] = 125;
    gsmIntData[1] = 125;
    gsmIntData[2] = 125;
    gsmIntData[3] = 125;
    gsmIntData[4] = 125;
    gsmIntData[5] = 125;
    gsmIntData[6] = 125;
    gsmIntData[7] = 125;
    gsmIntData[8] = 125;
    for(int j=8; j < MAX_THREADS; j++) gsmIntData[j] = 125;

    gInt = 25;
  }

  GroupMemoryBarrierWithGroupSync();

  float4 outval = 0.0f.xxxx;
  int u = int(gid.x);

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
    float value = (1.0f + GetGSMValue(gid.x)) * (1.0f + GetGSMValue(gid.x ^ 1));

    GroupMemoryBarrierWithGroupSync();
    gsmData[gid.x] = value;
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
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedAdd(gsmIntData[u], value);
    InterlockedAdd(gsmIntData[u], -value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(4))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedAnd(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(5))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedOr(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(6))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedXor(gsmIntData[u], value);
    InterlockedXor(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(7))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedMin(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(8))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedMax(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(9))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    int original;
    InterlockedExchange(gsmIntData[u], value, original);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(10))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    int original;
    InterlockedCompareExchange(gsmIntData[u], value, value+1, original);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(11))
  {
    int value = (int)(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedCompareStore(gsmIntData[u], value, value+1);
    GroupMemoryBarrierWithGroupSync();
    outval.x = (float)GetGSMIntValue(u+0);
    outval.y = (float)GetGSMIntValue(u+1);
    outval.z = (float)GetGSMIntValue(u+2);
    outval.w = (float)GetGSMIntValue(u+3);
  }
  else if (IsTest(12))
  {
    GroupMemoryBarrierWithGroupSync();
    outval.x = gInt;
    GroupMemoryBarrierWithGroupSync();
    InterlockedAdd(gInt,1);
    GroupMemoryBarrierWithGroupSync();
    outval.y = gInt;
    GroupMemoryBarrierWithGroupSync();
    InterlockedAdd(gInt,1);
    GroupMemoryBarrierWithGroupSync();
    outval.z = gInt;
    GroupMemoryBarrierWithGroupSync();
    InterlockedAdd(gInt,1);
    GroupMemoryBarrierWithGroupSync();
    outval.w = gInt;
  }
  else if (IsTest(13))
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

    ID3D12RootSignaturePtr rs = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0, 1),
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 1),
    });

    ID3DBlobPtr cs = Compile(comp, "main", "cs_5_0", CompileOptionFlags::SkipOptimise);

    ID3D12PipelineStatePtr pso50 = MakePSO().CS(cs).RootSig(rs);
    ID3D12PipelineStatePtr pso60;

    if(m_DXILSupport)
    {
      cs = Compile(comp, "main", "cs_6_0", CompileOptionFlags::SkipOptimise);

      pso60 = MakePSO().CS(cs).RootSig(rs);
    }

    float values[64];
    for(int i = 0; i < 64; i++)
      values[i] = RANDF(1.0f, 100.0f);
    ID3D12ResourcePtr inBuf = MakeBuffer().Data(values).UAV();
    ID3D12ResourcePtr outBuf = MakeBuffer().Size(sizeof(Vec4f) * 64 * 2).UAV();

    D3D12_GPU_DESCRIPTOR_HANDLE outUAVGPU =
        MakeUAV(outBuf).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateGPU(10);
    D3D12_CPU_DESCRIPTOR_HANDLE outUAVClearCPU =
        MakeUAV(outBuf).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateClearCPU(10);

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
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      UINT zero[4] = {};
      D3D12_RECT rect = {0, 0, sizeof(Vec4f) * 64, 1};

      ClearRenderTargetView(cmd, BBRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->SetComputeRootSignature(rs);
      cmd->SetComputeRootUnorderedAccessView(1, inBuf->GetGPUVirtualAddress());
      cmd->SetComputeRootUnorderedAccessView(2, outBuf->GetGPUVirtualAddress());

      pushMarker(cmd, "SM5");
      cmd->SetPipelineState(pso50);

      for(int i = 0; i < numCompTests; ++i)
      {
        ResourceBarrier(cmd);
        cmd->ClearUnorderedAccessViewUint(outUAVGPU, outUAVClearCPU, outBuf, zero, 1, &rect);
        ResourceBarrier(cmd);
        cmd->SetComputeRoot32BitConstant(0, i, 0);
        cmd->Dispatch(1, 1, 1);
      }

      popMarker(cmd);

      if(pso60)
      {
        pushMarker(cmd, "SM6");
        cmd->SetPipelineState(pso60);

        for(int i = 0; i < numCompTests; ++i)
        {
          ResourceBarrier(cmd);
          cmd->ClearUnorderedAccessViewUint(outUAVGPU, outUAVClearCPU, outBuf, zero, 1, &rect);
          ResourceBarrier(cmd);
          cmd->SetComputeRoot32BitConstant(0, i, 0);
          cmd->Dispatch(1, 1, 1);
        }
        popMarker(cmd);
      }

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
