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

TEST(D3D11_Discard_View, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Test that discards an RTV";

  std::string pixel = R"EOSHADER(

cbuffer consts : register(b0)
{
	float4 col;
};

float4 main() : SV_Target0
{
	return col;
}

)EOSHADER";

  int main()
  {
    d3d11_1 = true;

    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DFullscreenQuadVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11Texture2DPtr tex_rt =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, screenWidth, screenHeight).RTV();
    ID3D11RenderTargetViewPtr rtv = MakeRTV(tex_rt);

    Vec4f col;
    ID3D11BufferPtr cb = MakeBuffer().Constant().Size(sizeof(Vec4f));

    D3D11_VIEWPORT view[10];
    for(int i = 0; i < 10; i++)
    {
      view[i].MinDepth = 0.0f;
      view[i].MaxDepth = 1.0f;
      view[i].TopLeftX = (float)i * 50.0f;
      view[i].TopLeftY = 0.0f;
      view[i].Width = 50.0f;
      view[i].Height = 250.0f;
    }

    D3D11_VIEWPORT fullview;
    {
      fullview.MinDepth = 0.0f;
      fullview.MaxDepth = 1.0f;
      fullview.TopLeftX = 0.0f;
      fullview.TopLeftY = 0.0f;
      fullview.Width = (float)screenWidth;
      fullview.Height = (float)screenHeight;
    }

    while(Running())
    {
      ctx1->DiscardView(bbRTV);

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->PSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());

      {
        ctx->RSSetViewports(1, &fullview);

        rtv = MakeRTV(tex_rt);

        ctx->OMSetRenderTargets(1, &rtv.GetInterfacePtr(), NULL);

        col = Vec4f(RANDF(0.0f, 1.0f), RANDF(0.0f, 1.0f), RANDF(0.0f, 1.0f), 1.0f);
        ctx->UpdateSubresource(cb, 0, NULL, &col, sizeof(col), sizeof(col));

        ctx->Draw(4, 0);
      }

      for(int i = 0; i < 10; i++)
      {
        ctx->RSSetViewports(1, view + i);

        rtv = MakeRTV(tex_rt);

        ctx->OMSetRenderTargets(1, &rtv.GetInterfacePtr(), NULL);

        col = Vec4f(RANDF(0.0f, 1.0f), RANDF(0.0f, 1.0f), RANDF(0.0f, 1.0f), 1.0f);
        ctx->UpdateSubresource(cb, 0, NULL, &col, sizeof(col), sizeof(col));

        ctx->Draw(4, 0);
      }

      ctx->CopyResource(bbTex, tex_rt);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
