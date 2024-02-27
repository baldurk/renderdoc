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

RD_TEST(D3D12_Mesh_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Draws some primitives for testing the mesh view.";

  std::string vertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float2 col2 : COLOR0;
	float4 col : COLOR1;
};

cbuffer consts : register(b0)
{
  float4 scale;
  float4 offset;
};

v2f main(vertin IN, uint vid : SV_VertexID, uint inst : SV_InstanceID)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.xy * scale.xy + offset.xy, IN.pos.z, 1.0f);
	OUT.col = IN.col;

  if(inst > 0)
  {
    OUT.pos *= 0.3f;
    OUT.pos.xy += 0.1f;
    OUT.col.x = 1.0f;
  }

  OUT.col2 = OUT.pos.xy;

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float2 col2 : COLOR0;
	float4 col : COLOR1;
};

float4 main(v2f IN) : SV_Target0
{
	return IN.col + 1.0e-20 * IN.col2.xyxy;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    const DefaultA2V test[] = {
        // single color quad
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        // points, to test vertex picking
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(70.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(170.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(70.0f, 70.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(test);

    Vec4f cbufferdata[] = {
        Vec4f(2.0f / (float)screenWidth, 2.0f / (float)screenHeight, 1.0f, 1.0f),
        Vec4f(-1.0f, -1.0f, 0.0f, 0.0f),
    };

    ID3D12RootSignaturePtr sig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0, sizeof(cbufferdata) / sizeof(uint32_t)),
    });

    D3D12PSOCreator creator = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).DSV(
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

    creator.GraphicsDesc.DepthStencilState.DepthEnable = TRUE;
    creator.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineStatePtr pso = creator;

    creator.GraphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

    ID3D12PipelineStatePtr pointspso = creator;

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr dsv = MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight)
                                .DSV()
                                .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      OMSetRenderTargets(cmd, {rtv}, MakeDSV(dsv).CreateCPU(0));

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearDepthStencilView(cmd, dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0);

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      cmd->SetGraphicsRoot32BitConstants(0, sizeof(cbufferdata) / sizeof(uint32_t), &cbufferdata, 0);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      // a previous draw for testing 'whole pass' rendering
      cmd->DrawInstanced(3, 1, 10, 0);

      setMarker(cmd, "Quad");

      // draw two instances so we can test rendering other instances
      cmd->DrawInstanced(6, 2, 0, 0);

      setMarker(cmd, "Points");

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

      cmd->SetPipelineState(pointspso);

      cmd->SetGraphicsRoot32BitConstants(0, sizeof(cbufferdata) / sizeof(uint32_t), &cbufferdata, 0);

      cmd->DrawInstanced(4, 1, 6, 0);

      setMarker(cmd, "Lines");

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

      cmd->DrawInstanced(4, 1, 6, 0);

      setMarker(cmd, "Stride 0");

      IASetVertexBuffer(cmd, vb, 0, 0);

      cmd->DrawInstanced(1, 1, 0, 0);

      cmd->SetPipelineState(pso);

      setMarker(cmd, "Empty");

      cmd->DrawInstanced(1, 0, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
