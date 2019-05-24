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

TEST(D3D11_Midframe_Create, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Tests creating resources mid-frame to make sure that they and their contents are "
      "correctly tracked.";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float4 uv : TEXCOORD0;
};

Texture2D<float4> tex;

float4 main(v2f IN) : SV_Target0
{
	clip(float2(1.0f, 1.0f) - IN.uv.xy);
	return tex.Load(int3(IN.uv.xyz*64.0f));
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    DefaultA2V triangle[] = {
        {Vec3f(-1.0f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(-0.8f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.7f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.7f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.4f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.4f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(-0.2f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.1f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.1f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(0.1f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        // row two

        {Vec3f(-1.0f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(-0.8f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.7f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.7f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.4f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.4f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(-0.2f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.1f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.1f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(0.1f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(triangle);

    // make a 'reference' texture to copy from
    ID3D11Texture2DPtr copysrctex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).RTV();
    ID3D11RenderTargetViewPtr copysrcrtv = MakeRTV(copysrctex);

    float copycol[] = {0.1f, 0.5f, 0.1f, 1.0f};
    ctx->ClearRenderTargetView(copysrcrtv, copycol);

    copysrcrtv = NULL;

    D3D11_BOX box = {16, 16, 0, 48, 48, 1};
    float *data = new float[64 * 64 * 4];

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      // create a texture in the middle of the frame
      ID3D11Texture2DPtr tex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).RTV().SRV();
      ID3D11ShaderResourceViewPtr srv = MakeSRV(tex);
      ID3D11RenderTargetViewPtr rtv = MakeRTV(tex);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      // clear to a colour
      float grey[] = {0.5f, 0.5f, 0.5f, 1.0f};
      ctx->ClearRenderTargetView(rtv, grey);

      // draw once
      ctx->Draw(3, 0);

      // fill with simple colour ramp
      for(size_t i = 0; i < 32 * 32; i++)
      {
        data[i * 4 + 0] = float(i * 32) / 32.0f;
        data[i * 4 + 1] = float(i % 32) / 32.0f;
        data[i * 4 + 2] = float(i) / float(32 * 32);
        data[i * 4 + 3] = 1.0f;
      }

      // set data in the middle
      ctx->UpdateSubresource(tex, 0, &box, data, 32 * sizeof(float) * 4, 32 * 32 * sizeof(float) * 4);

      // draw another time
      ctx->Draw(3, 3);

      // force destruction of texture
      srv = NULL;
      rtv = NULL;
      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());
      tex = NULL;

      // create another
      tex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).RTV().SRV();
      srv = MakeSRV(tex);
      rtv = MakeRTV(tex);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      // clear to a different colour
      float pink[] = {1.0f, 0.8f, 0.8f, 1.0f};
      ctx->ClearRenderTargetView(rtv, pink);

      // draw once
      ctx->Draw(3, 6);

      // fill with simple colour ramp
      for(size_t i = 0; i < 64 * 64; i++)
      {
        data[i * 4 + 0] = float(i * 64) / 64.0f;
        data[i * 4 + 1] = float(i % 64) / 64.0f;
        data[i * 4 + 2] = float(i) / float(64 * 64);
        data[i * 4 + 3] = 1.0f;
      }

      ctx->UpdateSubresource(tex, 0, NULL, data, 64 * sizeof(float) * 4, 64 * 64 * sizeof(float) * 4);

      // fetch a deferred context and do the next draw on it.
      ID3D11DeviceContextPtr defctx;
      dev->CreateDeferredContext(0, &defctx);
      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      defctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      defctx->IASetInputLayout(defaultLayout);
      defctx->VSSetShader(vs, NULL, 0);
      defctx->PSSetShader(ps, NULL, 0);
      D3D11_VIEWPORT view = {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f};
      defctx->RSSetViewports(1, &view);
      defctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);
      defctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());
      defctx->Draw(3, 9);
      ID3D11CommandListPtr cmdList;
      defctx->FinishCommandList(TRUE, &cmdList);
      ctx->ExecuteCommandList(cmdList, TRUE);

      // destroy the command list and deferred context before present
      cmdList = NULL;
      defctx = NULL;

      // force destruction of texture
      srv = NULL;
      rtv = NULL;
      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());
      tex = NULL;

      // create another
      tex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).RTV().SRV();
      srv = MakeSRV(tex);
      rtv = MakeRTV(tex);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      // clear to a different colour
      float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
      ctx->ClearRenderTargetView(rtv, white);

      // draw once
      ctx->Draw(3, 12);

      // copy from reference source
      ctx->CopyResource(tex, copysrctex);

      // draw again
      ctx->Draw(3, 15);

      // force destruction of texture
      srv = NULL;
      rtv = NULL;
      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());
      tex = NULL;

      // create another
      tex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).RTV().SRV();
      srv = MakeSRV(tex);
      rtv = MakeRTV(tex);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      // clear to a different colour
      float blue[] = {0.2f, 0.2f, 0.6f, 1.0f};
      ctx->ClearRenderTargetView(rtv, blue);

      // draw once
      ctx->Draw(3, 18);

      // copy partially from reference source
      ctx->CopySubresourceRegion(tex, 0, 16, 16, 0, copysrctex, 0, &box);

      // draw again
      ctx->Draw(3, 21);

      Present();
    }

    delete[] data;

    return 0;
  }
};

REGISTER_TEST();
