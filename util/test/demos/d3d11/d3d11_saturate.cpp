/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

TEST(D3D11_Saturate, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests using saturate, originally for a bug report";

  std::string pixel = R"EOSHADER(

void main(float4 pos : SV_Position, out float4 a : SV_Target0, out float4 b : SV_Target1)
{
  // this code is arbitrary, just to get a negative value and ensure
  // it's a) not known ahead of time at all
  // b) not merged in with any of the calculations pre-saturate below
  float negative = log2(pos.x / 1000.0f);

  // maps to mov_sat
  float zero = saturate(negative);
  // maps to add_sat which breaks
  float addsatzero = saturate(negative - 1.00001f);
  // maps to mul_sat
  float mulsatzero = saturate(negative * 1.00001f);

  a.x = negative;
  a.y = zero;
  a.z = addsatzero;
  a.w = mulsatzero;

  b.x = float(zero == 0.0f);
  b.y = float(addsatzero == 0.0f);
  b.z = float(mulsatzero == 0.0f);
  b.w = 0.0f;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DFullscreenQuadVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11Texture2DPtr fltTex[2] = {
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 400, 400).RTV(),
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 400, 400).RTV(),
    };
    ID3D11RenderTargetViewPtr fltRT[2] = {
        MakeRTV(fltTex[0]), MakeRTV(fltTex[1]),
    };

    while(Running())
    {
      ClearRenderTargetView(fltRT[0], {0.4f, 0.5f, 0.6f, 1.0f});
      ClearRenderTargetView(fltRT[1], {0.4f, 0.5f, 0.6f, 1.0f});
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ID3D11RenderTargetView *rts[] = {
          fltRT[0], fltRT[1],
      };
      ctx->OMSetRenderTargets(2, rts, NULL);

      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
