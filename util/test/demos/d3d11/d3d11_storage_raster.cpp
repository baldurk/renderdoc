/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2026 Baldur Karlsson
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

RD_TEST(D3D11_Storage_Raster, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Testing stores to images from a rasterized draw with no rasterized outputs.";

  std::string vertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f main(vertin IN)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.xyz, 1);
	OUT.col = IN.col;
	OUT.uv = IN.uv;

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

RWByteAddressBuffer rwbuf : register(u0);

void main(v2f IN)
{
  uint orig = 0;
  rwbuf.InterlockedMin(0, uint(IN.pos.x), orig);
  rwbuf.InterlockedMin(4, uint(IN.pos.y), orig);
  rwbuf.InterlockedMax(8, uint(IN.pos.x), orig);
  rwbuf.InterlockedMax(12, uint(IN.pos.y), orig);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11VertexShaderPtr vs = CreateVS(Compile(vertex, "main", "vs_5_0"));
    ID3D11PixelShaderPtr ps = CreatePS(Compile(pixel, "main", "ps_5_0"));

    ID3D11BufferPtr buf = MakeBuffer().Size(64).ByteAddressed().UAV().SRV();
    ID3D11UnorderedAccessViewPtr uav = MakeUAV(buf).Format(DXGI_FORMAT_R32_TYPELESS);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      ClearUnorderedAccessView(uav, Vec4u(150, 150, 150, 150));

      IASetVertexBuffer(DefaultTriVB, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissor({0, 0, screenWidth, screenHeight});

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 1, &uav.GetInterfacePtr(),
                                                     NULL);

      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
