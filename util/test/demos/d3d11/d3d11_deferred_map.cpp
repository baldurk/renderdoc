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

RD_TEST(D3D11_Deferred_Map, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test that does Map() on a deferred context on buffers and textures.";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D<float4> tex;

float4 main(v2f IN) : SV_Target0
{
	clip(float2(0.9999f, 0.9999f) - IN.uv.xy);
	return tex.Load(int3(IN.uv.xy*64.0f, 0));
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11DeviceContextPtr defctx;
    dev->CreateDeferredContext(0, &defctx);

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    DefaultA2V triangle[] = {
        {Vec3f(-1.0f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, 2.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(3.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-1.0f, -1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(3.0f, -1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(triangle);

    ID3D11Texture2DPtr tex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).SRV().Mips(1).Mappable();
    ID3D11ShaderResourceViewPtr srv = MakeSRV(tex);

    ID3D11CommandListPtr cmdList;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    Vec4f *data = NULL;
    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      // set mip 0 to green on the deferred context
      defctx->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

      data = (Vec4f *)mapped.pData;
      for(int y = 0; y < 64; y++)
        for(int x = 0; x < 64; x++)
          data[y * 64 + x] = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

      defctx->Unmap(tex, 0);

      defctx->FinishCommandList(TRUE, &cmdList);

      // set mip 0 to red on the immediate context
      ctx->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

      data = (Vec4f *)mapped.pData;
      for(int y = 0; y < 64; y++)
        for(int x = 0; x < 64; x++)
          data[y * 64 + x] = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);

      ctx->Unmap(tex, 0);

      ctx->Draw(3, 0);

      ctx->ExecuteCommandList(cmdList, TRUE);

      ctx->Draw(3, 3);

      cmdList = NULL;

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
