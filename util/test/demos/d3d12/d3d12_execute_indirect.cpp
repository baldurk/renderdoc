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

#include "d3d12_test.h"

RD_TEST(D3D12_Execute_Indirect, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests use of ExecuteIndirect() in different edge-case scenarios.";

  std::string vert = R"EOSHADER(

struct vertin
{
  float4 pos : POSITION;
  float4 col : COLOR0;
};

struct v2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
};

cbuffer test : register(b0)
{
   float4 cbtest[5];
}

StructuredBuffer<float4> srvtest : register(t0);

RWStructuredBuffer<float4> uavtest : register(u0);

v2f main(vertin IN, uint vid : SV_VertexID)
{
  v2f OUT = (v2f)0;

  if(vid < 3)
  {
    OUT.pos = float4(IN.pos.xyz, 1);
    OUT.col = IN.col;

    if(cbtest[1].w != 1.234f)
      OUT.col.r += 0.1f;

    if(srvtest[1].w != 1.234f)
      OUT.col.r += 0.2f;

    if(uavtest[1].w != 1.234f)
      OUT.col.r += 0.5f;
  }
  else
  {
    float4 positions[] = {
      float4(-0.5f, -0.5f, 0.0f, 1.0f),
      float4( 0.0f,  0.5f, 0.0f, 1.0f),
      float4( 0.5f, -0.5f, 0.0f, 1.0f),
    };

    OUT.pos = positions[vid-3];
    OUT.pos.x += 0.5f;
    OUT.col = float4(1,0,1,1);
  }

  return OUT;
}

)EOSHADER";

  std::string vert2 = R"EOSHADER(

struct vertin
{
  float4 pos : POSITION;
  float4 col : COLOR0;
};

struct v2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
};

v2f main(vertin IN, uint vid : SV_VertexID)
{
  v2f OUT = (v2f)0;

  OUT.pos = float4(IN.pos.xyz, 1);
  OUT.col = IN.col;

  return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

struct v2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
};

float4 main(v2f IN) : SV_Target0
{
  return IN.col;
}

)EOSHADER";

  std::string comp = R"EOSHADER(

RWStructuredBuffer<float4> bufout : register(u0);

RWByteAddressBuffer customvbargs : register(u1);

[numthreads(1,1,1)]
void main(uint3 gid : SV_GroupID)
{
  uint tid = gid.z*30*12 + gid.y*12 + gid.x;

  bufout[tid] = float4(gid, tid);

  // try to pick some threads that will race to fill in the draw parameters

  // ignore the first set of threadgroups
  if(tid < 300)
    return;

  tid -= 300;

  // pick one threadgroup out of every 128
  if((tid % 128) != 17)
    return;
  tid /= 128;

  // now pick the first 8
  if(tid >= 8)
    return;

  const uint drawStride = 16;
  const uint vertStride = 32;
  const uint paddingVerts = 15;
  const uint vbStart = 256;

  uint numVerts = 3*(1+tid);
  uint startVtx = 100*tid + 5;

  uint drawslot;
  customvbargs.InterlockedAdd(0, 1, drawslot);

  uint vbdataslot;
  customvbargs.InterlockedAdd(4, vertStride*(numVerts+paddingVerts*2), vbdataslot);

  customvbargs.Store4((1+drawslot)*drawStride, uint4(numVerts, 1, (vbdataslot / vertStride) + 15, vbdataslot));

  // first fill our range with invalid vertices that will show up
  for(uint vert = 0; vert < numVerts+paddingVerts*2; vert++)
  {
    float2 pos;
    switch(vert % 4)
    {
      default:
      case 0:
        pos.x = 1100.0f; pos.y = 0.6f; break;
      case 1:
        pos.x = -1200.0f; pos.y = 0.2f; break;
      case 2:
        pos.x = 1300.0f; pos.y = -0.2f; break;
      case 3:
        pos.x = -1400.0f; pos.y = -0.6f; break;
    }

    customvbargs.Store4(vbStart + vbdataslot + vert*vertStride, asuint(float4(pos.x, pos.y, vert, 0)));
    customvbargs.Store4(vbStart + vbdataslot + vert*vertStride + 16, asuint(float4(1.0f, 0.0f, 1.0f, 1.0f)));
  }

  // skip the 'middle' in the 9 space
  if(tid >= 4) tid++;
  
  float2 origin = float2(float(tid%3)/2.0f, 1.0f - float(tid/3)/2.0f);

  // squeeze in a bit towards the centre
  origin = ((origin * 2.0f - 1.0f.xx) * 0.75f.xx);

  float2 pos[24];
  for(int tri=0; tri < 8; tri++)
  {
    float x = (float(tri)/8.0f)*300.0f;

    pos[tri*3+0] = float2(0.0f, 0.0f);
    pos[tri*3+1] = float2(sin(radians(x)), cos(radians(x)));
    pos[tri*3+2] = float2(sin(radians(x+30.0f)), cos(radians(x+30.0f)));
  }

  // now fill in just the correct vertices
  for(uint i=0; i < numVerts; i++)
  {
    customvbargs.Store4(vbStart + vbdataslot + (paddingVerts + i)*vertStride, asuint(float4(origin.x+pos[i].x*0.2f, origin.y+pos[i].y*0.2f, 0, 0)));
    customvbargs.Store4(vbStart + vbdataslot + (paddingVerts + i)*vertStride + 16, asuint(float4(0.0f, 1.0f, 0.0f, 1.0f)));
  }
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vert, "main", "vs_5_0");
    ID3DBlobPtr vs2blob = Compile(vert2, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    const D3D12_INPUT_CLASSIFICATION vertex = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    static const std::vector<D3D12_INPUT_ELEMENT_DESC> layout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, vertex, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, vertex, 0},
    };

    struct A2V
    {
      Vec4f pos;
      Vec4f col;
    };

    const A2V tri[9] = {
        {Vec4f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f)},
        {Vec4f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f)},
        {Vec4f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f)},
    };

    float checkdata[1024] = {};
    checkdata[64 + 7] = 1.234f;

    ID3D12ResourcePtr vb = MakeBuffer().Data(tri);
    ID3D12ResourcePtr cbv = MakeBuffer().Data(checkdata);
    ID3D12ResourcePtr srv = MakeBuffer().Data(checkdata);
    ID3D12ResourcePtr uav = MakeBuffer().UAV().Data(checkdata);

    ID3D12RootSignaturePtr patchsig = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0),
        srvParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0),
        uavParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0),
    });

    ID3D12CommandSignaturePtr patchArgSig =
        MakeCommandSig(patchsig, {vbArg(0), cbvArg(0), srvArg(1), uavArg(2), drawArg()});

    struct PatchArgs
    {
      D3D12_VERTEX_BUFFER_VIEW vb;
      D3D12_GPU_VIRTUAL_ADDRESS cbv;
      D3D12_GPU_VIRTUAL_ADDRESS srv;
      D3D12_GPU_VIRTUAL_ADDRESS uav;
      D3D12_DRAW_ARGUMENTS draw;
    } patchargs;

    patchargs.vb.BufferLocation = vb->GetGPUVirtualAddress();
    patchargs.vb.SizeInBytes = sizeof(tri);
    patchargs.vb.StrideInBytes = sizeof(A2V);
    patchargs.cbv = cbv->GetGPUVirtualAddress() + 256;
    patchargs.srv = srv->GetGPUVirtualAddress() + 256;
    patchargs.uav = uav->GetGPUVirtualAddress() + 256;
    patchargs.draw.VertexCountPerInstance = 3;
    patchargs.draw.InstanceCount = 1;
    patchargs.draw.StartInstanceLocation = 0;
    patchargs.draw.StartVertexLocation = 0;

    ID3D12ResourcePtr patchArgBuf = MakeBuffer().Upload().Size(sizeof(patchargs)).Data(&patchargs);

    ID3D12PipelineStatePtr patchpso =
        MakePSO().RootSig(patchsig).InputLayout(layout).VS(vsblob).PS(psblob);

    ID3D12CommandSignaturePtr compArgSig = MakeCommandSig(NULL, {dispatchArg()});

    D3D12_DISPATCH_ARGUMENTS compargs;

    compargs.ThreadGroupCountX = 12;
    compargs.ThreadGroupCountY = 30;
    compargs.ThreadGroupCountZ = 10;

    ID3D12ResourcePtr compArgBuf = MakeBuffer().Upload().Size(sizeof(compargs)).Data(&compargs);

    ID3D12ResourcePtr compuav = MakeBuffer().UAV().Size(1024 * 1024 * 4);

    MakeUAV(compuav)
        .Format(DXGI_FORMAT_R32G32B32A32_UINT)
        .FirstElement(4096)
        .NumElements(256000)
        .CreateGPU(1);

    ID3D12RootSignaturePtr compsig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1, 0),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 1, 1),
    });

    ID3DBlobPtr csblob = Compile(comp, "main", "cs_5_0");

    ID3D12PipelineStatePtr comppso = MakePSO().RootSig(compsig).CS(csblob);

    ID3D12ResourcePtr customvbargs = MakeBuffer().Size(1024 * 1024 * 4);
    const uint32_t countDrawsInFullBuffer = 3;
    ID3D12ResourcePtr fullargsDrawBuf =
        MakeBuffer().Size(countDrawsInFullBuffer * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

    PatchArgs fullargsStateDraw[countDrawsInFullBuffer];
    for(uint32_t i = 0; i < countDrawsInFullBuffer; ++i)
      fullargsStateDraw[i] = patchargs;

    ID3D12ResourcePtr fullargsStateDrawBuf =
        MakeBuffer().Upload().Size(countDrawsInFullBuffer * sizeof(patchargs)).Data(fullargsStateDraw);

    ID3D12RootSignaturePtr plainsig = MakeSig({});
    ID3D12PipelineStatePtr plainpso =
        MakePSO().RootSig(plainsig).InputLayout(layout).VS(vs2blob).PS(psblob);

    ID3D12CommandSignaturePtr plainArgSig = MakeCommandSig(NULL, {drawArg()});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(
        customvbargs, D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    while(Running())
    {
      std::vector<ID3D12GraphicsCommandListPtr> cmds;

      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      Reset(cmd);
      cmds.push_back(cmd);

      uint32_t zero[4] = {};
      cmd->ClearUnorderedAccessViewUint(
          MakeUAV(compuav).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateGPU(0),
          MakeUAV(compuav).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateClearCPU(0), compuav, zero,
          0, NULL);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      pushMarker(cmd, "Multiple draws");
      for(int i = 0; i < 8; i++)
      {
        ClearRenderTargetView(cmd, rtv, {0.0f, 0.0f, 0.0f, 1.0f});

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmd->SetPipelineState(patchpso);
        cmd->SetGraphicsRootSignature(patchsig);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});

        cmd->ExecuteIndirect(patchArgSig, 1, patchArgBuf, 0, NULL, 0);
      }
      popMarker(cmd);

      setMarker(cmd, "Post draw");

      cmd->Close();
      cmd = GetCommandBuffer();
      Reset(cmd);
      cmds.push_back(cmd);

      setMarker(cmd, "Separate Post draw");

      pushMarker(cmd, "Single dispatch");
      {
        cmd->SetPipelineState(comppso);
        cmd->SetComputeRootSignature(compsig);

        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

        cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

        cmd->ExecuteIndirect(compArgSig, 1, compArgBuf, 0, NULL, 0);
      }
      popMarker(cmd);

      setMarker(cmd, "Post Single dispatch");

      cmd->Close();

      cmd = GetCommandBuffer();
      Reset(cmd);
      cmds.push_back(cmd);

      setMarker(cmd, "Separate Post Single dispatch");

      D3D12_RESOURCE_BARRIER barrier;
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.UAV.pResource = compuav;
      cmd->ResourceBarrier(1, &barrier);

      ResourceBarrier(cmd, compuav, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_COPY_SOURCE);

      cmd->CopyBufferRegion(customvbargs, 0, compuav, 0, 4 * 1024 * 1024);

      ResourceBarrier(
          cmd, customvbargs, D3D12_RESOURCE_STATE_COPY_DEST,
          D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

      cmd->Close();

      cmd = GetCommandBuffer();
      Reset(cmd);
      cmds.push_back(cmd);

      pushMarker(cmd, "Custom order draw");
      {
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW view;
        view.BufferLocation = customvbargs->GetGPUVirtualAddress() + 4096 * (sizeof(Vec4f)) + 256;
        view.StrideInBytes = sizeof(A2V);
        view.SizeInBytes = sizeof(A2V) * 1120;
        cmd->IASetVertexBuffers(0, 1, &view);

        cmd->SetPipelineState(plainpso);
        cmd->SetGraphicsRootSignature(plainsig);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});

        cmd->ExecuteIndirect(plainArgSig, 8, customvbargs, 4096 * (sizeof(Vec4f)) + sizeof(Vec4u),
                             NULL, 0);
      }
      popMarker(cmd);

      pushMarker(cmd, "Full Arg Buffer: Pure Draw");
      {
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW view;
        view.BufferLocation = customvbargs->GetGPUVirtualAddress() + 4096 * (sizeof(Vec4f)) + 256;
        view.StrideInBytes = sizeof(A2V);
        view.SizeInBytes = sizeof(A2V) * 1120;
        cmd->IASetVertexBuffers(0, 1, &view);

        cmd->SetPipelineState(plainpso);
        cmd->SetGraphicsRootSignature(plainsig);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});

        cmd->ExecuteIndirect(plainArgSig, countDrawsInFullBuffer, fullargsDrawBuf, 0, NULL, 0);
      }
      popMarker(cmd);

      pushMarker(cmd, "Full Arg Buffer: State + Draw");
      {
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmd->SetPipelineState(patchpso);
        cmd->SetGraphicsRootSignature(patchsig);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});
        cmd->ExecuteIndirect(patchArgSig, countDrawsInFullBuffer, fullargsStateDrawBuf, 0, NULL, 0);
      }
      popMarker(cmd);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit(cmds);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
