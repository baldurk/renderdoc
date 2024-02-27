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

RD_TEST(D3D12_Shader_Editing, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that shader editing works with different combinations of shader re-use.";

  std::string vertex = R"EOSHADER(

float4 main(float3 INpos : POSITION) : SV_Position
{
	float4 ret = float4(0,0,0,1);
  ret.xyz += INpos.xyz;
  return ret;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

float4 main() : SV_Target0
{
#if 1
	return float4(0.0, 1.0, 0.0, 1.0);
#else
	return float4(0.0, 1.0, 1.0, 1.0);
#endif
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    // since we assign shader IDs based on blob hash, we need to make this blob slightly different
    ID3DBlobPtr psblob2 = Compile(pixel + " ", "main", "ps_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).RTVs(
        {DXGI_FORMAT_R32G32B32A32_FLOAT});
    ID3D12PipelineStatePtr pso2 = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob2).RTVs(
        {DXGI_FORMAT_R32G32B32A32_FLOAT});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      D3D12_CPU_DESCRIPTOR_HANDLE offrtv = MakeRTV(rtvtex).CreateCPU(0);

      ClearRenderTargetView(cmd, offrtv, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearRenderTargetView(cmd, bbrtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetGraphicsRootSignature(sig);

      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {offrtv}, {});

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth / 2.0f, (float)screenHeight, 0.0f, 1.0f});
      cmd->SetPipelineState(pso);
      setMarker(cmd, "Draw 1");
      cmd->DrawInstanced(3, 1, 0, 0);

      RSSetViewport(cmd, {(float)screenWidth / 2.0f, 0.0f, (float)screenWidth / 2.0f,
                          (float)screenHeight, 0.0f, 1.0f});
      cmd->SetPipelineState(pso2);
      setMarker(cmd, "Draw 2");
      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
