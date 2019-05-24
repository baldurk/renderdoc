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

TEST(D3D11_Mip_RTV, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Test rendering into RTV mip levels";

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
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DFullscreenQuadVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11Texture2DPtr rt = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024);
    ID3D11RenderTargetViewPtr rtv[4] = {
        MakeRTV(rt).FirstMip(0), MakeRTV(rt).FirstMip(1), MakeRTV(rt).FirstMip(2),
        MakeRTV(rt).FirstMip(3),
    };

    Vec4f col;

    ID3D11BufferPtr cb = MakeBuffer().Constant().Size(sizeof(Vec4f));

    D3D11_VIEWPORT view0 = {0.0f, 0.0f, (float)1024.0f, (float)1024.0f, 0.0f, 1.0f};
    D3D11_VIEWPORT view1 = {0.0f, 0.0f, (float)512.0f, (float)512.0f, 0.0f, 1.0f};
    D3D11_VIEWPORT view2 = {0.0f, 0.0f, (float)256.0f, (float)256.0f, 0.0f, 1.0f};
    D3D11_VIEWPORT view3 = {0.0f, 0.0f, (float)128.0f, (float)128.0f, 0.0f, 1.0f};

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});
      for(int i = 0; i < 4; i++)
        ClearRenderTargetView(rtv[i], {0.4f, 0.5f, 0.6f, 1.0f});

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->PSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());

      col = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
      ctx->UpdateSubresource(cb, 0, NULL, &col, sizeof(col), sizeof(col));
      ctx->RSSetViewports(1, &view0);
      ctx->OMSetRenderTargets(1, &rtv[0].GetInterfacePtr(), NULL);

      ctx->Draw(4, 0);

      col = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      ctx->UpdateSubresource(cb, 0, NULL, &col, sizeof(col), sizeof(col));
      ctx->RSSetViewports(1, &view1);
      ctx->OMSetRenderTargets(1, &rtv[1].GetInterfacePtr(), NULL);

      ctx->Draw(4, 0);

      col = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      ctx->UpdateSubresource(cb, 0, NULL, &col, sizeof(col), sizeof(col));
      ctx->RSSetViewports(1, &view2);
      ctx->OMSetRenderTargets(1, &rtv[2].GetInterfacePtr(), NULL);

      ctx->Draw(4, 0);

      col = Vec4f(1.0f, 0.0f, 1.0f, 1.0f);
      ctx->UpdateSubresource(cb, 0, NULL, &col, sizeof(col), sizeof(col));
      ctx->RSSetViewports(1, &view3);
      ctx->OMSetRenderTargets(1, &rtv[3].GetInterfacePtr(), NULL);

      ctx->Draw(4, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
