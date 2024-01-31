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

RD_TEST(D3D11_Buffer_Truncation, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Tests using a constant buffer that is truncated by range (when supported), as well as "
      "vertex/index buffers truncated by size.";

  std::string vertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct v2f
{
	float4 svpos : SV_POSITION;
	float4 pos : OUTPOSITION;
	float4 col : OUTCOLOR;
};

v2f main(vertin IN)
{
	v2f OUT = (v2f)0;

	OUT.svpos = OUT.pos = float4(IN.pos.xyz, 1);
	OUT.col = IN.col;

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

cbuffer consts : register(b0)
{
  float4 padding[16];
  float4 outcol;
};

float4 main() : SV_Target0
{
	return outcol;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    const DefaultA2V OffsetTri[] = {
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(9.9f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(8.8f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };
    uint16_t indices[] = {99, 99, 99, 1, 2, 3, 4, 5};
    Vec4f cbufferdata[64] = {};
    cbufferdata[32] = Vec4f(1.0f, 2.0f, 3.0f, 4.0f);

    if(!opts.ConstantBufferOffsetting)
      cbufferdata[16] = Vec4f(1.0f, 2.0f, 3.0f, 4.0f);

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(OffsetTri);
    ID3D11BufferPtr ib = MakeBuffer().Index().Data(indices);
    ID3D11BufferPtr cb =
        MakeBuffer()
            .Constant()
            .Data(cbufferdata)
            .Size(opts.ConstantBufferOffsetting ? sizeof(cbufferdata) : sizeof(Vec4f) * 16);

    ID3D11Texture2DPtr fltTex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight).RTV().SRV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), sizeof(DefaultA2V) * 3);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, sizeof(uint16_t) * 3);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      if(opts.ConstantBufferOffsetting)
      {
        UINT offset = 16;
        UINT count = 16;
        ctx1->PSSetConstantBuffers1(0, 1, &cb.GetInterfacePtr(), &offset, &count);
      }
      else
      {
        setMarker("NoCBufferRange");
        ctx->PSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());
      }

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &fltRT.GetInterfacePtr(), NULL);

      ctx->DrawIndexed(6, 0, 0);

      blitToSwap(fltTex);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
