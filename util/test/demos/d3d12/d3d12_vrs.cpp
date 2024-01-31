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

RD_TEST(D3D12_VRS, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Checks that VRS is correctly replayed and that state is inspectable";

  std::string pixel = R"EOSHADER(

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float4 main(float4 pos : SV_Position) : SV_Target0
{
  uint col = wang_hash(uint(pos.x * 10000.0f + pos.y));
  float4 outcol;
  outcol.x = float((col & 0xff000000u) >> 24u) / 255.0f;
  outcol.y = float((col & 0x00ff0000u) >> 16u) / 255.0f;
  outcol.z = float((col & 0x0000ff00u) >>  8u) / 255.0f;
  outcol.w = 1.0f;
	return outcol;
}

)EOSHADER";

  std::string vertex = R"EOSHADER(

struct OUT
{
float4 pos : SV_Position;

#ifdef VERT_VRS
uint rate : SV_ShadingRate;
#endif
};

OUT main(float3 pos : POSITION, float4 col : COLOR0)
{
	OUT o = (OUT)0;

	o.pos = float4(pos.xyz, 1);

#ifdef VERT_VRS
  o.rate = uint(col.x) << 2 | uint(col.y);
#endif

	return o;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(opts6.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED)
      Avail = "Variable shading rate is not supported";
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ID3D12PipelineStatePtr vertpso;
    // without DXIL we can't compile shaders with shading rate exported from the vertex
    if(m_DXILSupport)
    {
      vsblob = Compile("#define VERT_VRS 1\n\n" + vertex, "main", "vs_6_4");
      psblob = Compile(pixel, "main", "ps_6_0");

      vertpso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);
    }

    const DefaultA2V tris[6] = {
        {Vec3f(-1.0f, -0.6f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.4f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, -0.6f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.0f, -0.4f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, 0.6f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.0f, -0.4f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(tris);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    if(opts6.ShadingRateImageTileSize == 0)
      opts6.ShadingRateImageTileSize = 1;

    ID3D12ResourcePtr shadImage =
        MakeTexture(DXGI_FORMAT_R8_UINT, screenWidth / opts6.ShadingRateImageTileSize,
                    screenHeight / opts6.ShadingRateImageTileSize)
            .Mips(1)
            .UAV()
            .InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      if(opts6.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)
      {
        D3D12_RECT rect = {0, 0, LONG(screenWidth / opts6.ShadingRateImageTileSize),
                           LONG(screenHeight / opts6.ShadingRateImageTileSize)};
        uint32_t col[4] = {};
        col[0] = D3D12_SHADING_RATE_2X2;

        D3D12_CPU_DESCRIPTOR_HANDLE shadCPU = MakeUAV(shadImage).CreateClearCPU(1);
        D3D12_GPU_DESCRIPTOR_HANDLE shadGPU = MakeUAV(shadImage).CreateGPU(1);
        cmd->ClearUnorderedAccessViewUint(shadGPU, shadCPU, shadImage, col, 1, &rect);

        col[0] = D3D12_SHADING_RATE_1X1;
        rect.left = LONG(screenWidth / opts6.ShadingRateImageTileSize) -
                    LONG((screenWidth / 8) / opts6.ShadingRateImageTileSize);
        cmd->ClearUnorderedAccessViewUint(shadGPU, shadCPU, shadImage, col, 1, &rect);

        ResourceBarrier(cmd, shadImage, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
      }

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetGraphicsRootSignature(sig);

      OMSetRenderTargets(cmd, {rtv}, {});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      float x = (float)screenWidth / 4.0f;
      float y = (float)screenHeight / 4.0f;

      cmd->SetPipelineState(pso);

      ID3D12GraphicsCommandList5Ptr cmd5 = cmd;

      D3D12_SHADING_RATE_COMBINER combiners[] = {
          D3D12_SHADING_RATE_COMBINER_MAX,
          D3D12_SHADING_RATE_COMBINER_MAX,
      };

      pushMarker(cmd, "First");

      {
        setMarker(cmd, "Default");

        RSSetViewport(cmd, {x * 0.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);
      }

      {
        setMarker(cmd, "Base");

        cmd5->RSSetShadingRate(D3D12_SHADING_RATE_2X2, combiners);

        RSSetViewport(cmd, {x * 1.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);

        cmd5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);
      }

      if(opts6.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)
      {
        setMarker(cmd, "Vertex");

        cmd->SetPipelineState(vertpso);

        RSSetViewport(cmd, {x * 2.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);

        cmd->SetPipelineState(pso);
        setMarker(cmd, "Image");

        cmd5->RSSetShadingRateImage(shadImage);

        RSSetViewport(cmd, {x * 3.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);

        cmd5->RSSetShadingRateImage(NULL);

        setMarker(cmd, "Base + Vertex");

        cmd5->RSSetShadingRate(D3D12_SHADING_RATE_2X2, combiners);
        cmd->SetPipelineState(vertpso);

        RSSetViewport(cmd, {x * 0.0f, y, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);

        cmd->SetPipelineState(pso);
        cmd5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);

        setMarker(cmd, "Base + Image");

        cmd5->RSSetShadingRate(D3D12_SHADING_RATE_2X2, combiners);
        cmd5->RSSetShadingRateImage(shadImage);

        RSSetViewport(cmd, {x * 3.0f, y, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);

        cmd5->RSSetShadingRateImage(NULL);
        cmd5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);

        setMarker(cmd, "Vertex + Image");

        cmd5->RSSetShadingRateImage(shadImage);
        cmd->SetPipelineState(vertpso);

        RSSetViewport(cmd, {x * 3.0f, y * 2.0f, x, y, 0.0f, 1.0f});
        cmd->DrawInstanced(6, 1, 0, 0);

        cmd->SetPipelineState(pso);
        cmd5->RSSetShadingRateImage(NULL);

        ResourceBarrier(cmd, shadImage, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      }

      popMarker(cmd);

      cmd->Close();
      cmd5 = NULL;

      ID3D12GraphicsCommandListPtr cmdB = GetCommandBuffer();

      Reset(cmdB);

      cmdB->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      cmdB->SetGraphicsRootSignature(sig);
      cmdB->SetPipelineState(pso);

      OMSetRenderTargets(cmdB, {rtv}, {});
      RSSetScissorRect(cmdB, {0, 0, screenWidth, screenHeight});
      RSSetViewport(cmdB, {0.0f, 0.0f, x, y, 0.0f, 1.0f});

      pushMarker(cmdB, "Second");

      {
        setMarker(cmdB, "Default");

        RSSetViewport(cmdB, {x * 0.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(6, 1, 0, 0);
      }

      {
        setMarker(cmdB, "Base");

        RSSetViewport(cmdB, {x * 1.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(0, 0, 0, 0);
      }

      if(opts6.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)
      {
        setMarker(cmdB, "Vertex");

        RSSetViewport(cmdB, {x * 2.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(0, 0, 0, 0);

        setMarker(cmdB, "Image");

        RSSetViewport(cmdB, {x * 3.0f, 0.0f, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(0, 0, 0, 0);

        setMarker(cmdB, "Base + Vertex");

        RSSetViewport(cmdB, {x * 0.0f, y, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(0, 0, 0, 0);

        setMarker(cmdB, "Base + Image");

        RSSetViewport(cmdB, {x * 3.0f, y, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(0, 0, 0, 0);

        setMarker(cmdB, "Vertex + Image");

        RSSetViewport(cmdB, {x * 3.0f, y * 2.0f, x, y, 0.0f, 1.0f});
        cmdB->DrawInstanced(0, 0, 0, 0);
      }

      popMarker(cmdB);

      FinishUsingBackbuffer(cmdB, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmdB->Close();

      Submit({cmd, cmdB});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
