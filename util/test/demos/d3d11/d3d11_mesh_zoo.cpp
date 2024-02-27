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

#include "d3d11_test.h"

RD_TEST(D3D11_Mesh_Zoo, D3D11GraphicsTest)
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

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

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

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(test);

    Vec4f cbufferdata[] = {
        Vec4f(2.0f / (float)screenWidth, 2.0f / (float)screenHeight, 1.0f, 1.0f),
        Vec4f(-1.0f, -1.0f, 0.0f, 0.0f),
    };

    ID3D11BufferPtr cb = MakeBuffer().Constant().Data(cbufferdata);

    ID3D11Texture2DPtr bbDepth =
        MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight).DSV();
    ID3D11DepthStencilViewPtr bbDSV = MakeDSV(bbDepth);

    CD3D11_DEPTH_STENCIL_DESC dd = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS;
    dd.StencilEnable = FALSE;
    dd.StencilWriteMask = dd.StencilReadMask = 0xff;

    ID3D11DepthStencilStatePtr ds;
    CHECK_HR(dev->CreateDepthStencilState(&dd, &ds));

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      ctx->ClearDepthStencilView(bbDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);
      ctx->OMSetDepthStencilState(ds, 0);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->VSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), bbDSV);

      // a previous draw for testing 'whole pass' rendering
      ctx->Draw(3, 10);

      setMarker("Quad");

      // draw two instances so we can test rendering other instances
      ctx->DrawInstanced(6, 2, 0, 0);

      setMarker("Points");

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

      ctx->Draw(4, 6);

      setMarker("Lines");

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

      ctx->Draw(4, 6);

      setMarker("Stride 0");
      IASetVertexBuffer(vb, 0, 0);

      ctx->Draw(1, 0);

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      setMarker("Empty");
      ctx->DrawInstanced(0, 0, 0, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
