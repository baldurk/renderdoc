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

TEST(D3D11_Overdraw_Stress, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Renders a lot of overlapping triangles";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11Texture2DPtr bbDepth =
        MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight).DSV();
    ID3D11DepthStencilViewPtr bbDSV = MakeDSV(bbDepth);

    CD3D11_RASTERIZER_DESC rd = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
    rd.CullMode = D3D11_CULL_NONE;

    ID3D11RasterizerStatePtr rs;
    CHECK_HR(dev->CreateRasterizerState(&rd, &rs));

    CD3D11_BLEND_DESC bd = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    bd.IndependentBlendEnable = TRUE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;
    bd.RenderTarget[0].RenderTargetWriteMask = 0xf;

    ID3D11BlendStatePtr bs;
    CHECK_HR(dev->CreateBlendState(&bd, &bs));

    CD3D11_DEPTH_STENCIL_DESC dd = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
    dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS;
    dd.StencilEnable = FALSE;
    dd.StencilWriteMask = dd.StencilReadMask = 0xff;

    ID3D11DepthStencilStatePtr ds;
    CHECK_HR(dev->CreateDepthStencilState(&dd, &ds));

    const size_t numVerts = 1200;

    DefaultA2V triangle[numVerts] = {0};

    for(int i = 0; i < numVerts; i++)
    {
      triangle[i].pos.x = ((float(rand()) / float(RAND_MAX)) - 0.5f) * 2.0f;
      triangle[i].pos.y = ((float(rand()) / float(RAND_MAX)) - 0.5f) * 2.0f;
      triangle[i].pos.z = ((float(rand()) / float(RAND_MAX)) - 0.5f) * 2.0f;

      triangle[i].col.x = float(rand()) / float(RAND_MAX);
      triangle[i].col.y = float(rand()) / float(RAND_MAX);
      triangle[i].col.z = float(rand()) / float(RAND_MAX);
    }

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(triangle);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      ctx->ClearDepthStencilView(bbDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      ctx->RSSetState(rs);

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);
      float bf[] = {1.0f, 0.0f, 1.0f, 0.0f};
      ctx->OMSetBlendState(bs, bf, ~0U);
      ctx->OMSetDepthStencilState(ds, 0);

      ctx->Draw(numVerts, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
