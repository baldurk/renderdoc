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

#include "d3d12_test.h"

RD_TEST(D3D12_Vertex_UAV, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Runs some tests with a vertex shader visible UAV to test that root signature patching for "
      "any PS UAVs works correctly";

  std::string vertex = R"EOSHADER(

#ifndef SPACE
#define SPACE space0
#endif

#if SM >= 51
RWByteAddressBuffer testUAV : register(u0, SPACE);
RWByteAddressBuffer testUAV2 : register(u1, SPACE);
#else
RWByteAddressBuffer testUAV : register(u0);
RWByteAddressBuffer testUAV2 : register(u1);
#endif

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f main(uint vid : SV_VertexID)
{
	float2 positions[] = {
		float2(-1.0f,  1.0f),
		float2( 1.0f,  1.0f),
		float2(-1.0f, -1.0f),
		float2( 1.0f, -1.0f),
	};

  float a = asfloat(testUAV.Load(16));
  float b = asfloat(testUAV2.Load(16));

  v2f ret;
	ret.pos = float4(positions[vid] * float2(a, b), 0, 1);
  ret.col = float4(a, b, 0, 1);
  ret.uv = float2(a, b);
  return ret;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        uavParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 1),
    });

    // 105202922 is the magic space renderdoc tries to use to avoid collisions
    ID3D12RootSignaturePtr collidesig = MakeSig({
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 105202922, 0),
        uavParam(D3D12_SHADER_VISIBILITY_VERTEX, 105202922, 1),
    });

    ID3D12PipelineStatePtr pso[3];
    ID3D12PipelineStatePtr collidepso[3];

    std::string smstring[] = {"5_0", "5_1", "6_0"};
    int smval[] = {50, 51, 60};
    for(int i = 0; i < 3; i++)
    {
      if(smval[i] == 60 && !m_DXILSupport)
        continue;

      std::string sm = smstring[i];

      std::string header = "#define SM " + std::to_string(smval[i]) + "\n\n";
      ID3DBlobPtr vsblob = Compile(header + vertex, "main", "vs_" + sm);
      header += "#define SPACE space105202922\n\n";
      ID3DBlobPtr collidevsblob = Compile(header + vertex, "main", "vs_" + sm);
      ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_" + sm);

      pso[i] = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

      if(smval[i] >= 51)
        collidepso[i] = MakePSO().RootSig(collidesig).InputLayout().VS(collidevsblob).PS(psblob);
    }

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr uav = MakeBuffer().Data(DefaultTri).UAV();

    ResourceBarrier(uav, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      for(int i = 0; i < 3; i++)
      {
        if(pso[i] == NULL)
          continue;

        setMarker(cmd, "Normal_" + smstring[i]);
        cmd->SetPipelineState(pso[i]);
        cmd->SetGraphicsRootSignature(sig);
        cmd->SetGraphicsRootUnorderedAccessView(0, uav->GetGPUVirtualAddress());
        cmd->SetGraphicsRootUnorderedAccessView(1, uav->GetGPUVirtualAddress());
        cmd->DrawInstanced(3, 1, 0, 0);

        if(collidepso[i] == NULL)
          continue;

        setMarker(cmd, "Collide_" + smstring[i]);
        cmd->SetPipelineState(collidepso[i]);
        cmd->SetGraphicsRootSignature(collidesig);
        cmd->SetGraphicsRootUnorderedAccessView(0, uav->GetGPUVirtualAddress());
        cmd->SetGraphicsRootUnorderedAccessView(1, uav->GetGPUVirtualAddress());
        cmd->DrawInstanced(3, 1, 0, 0);
      }

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
