/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

TEST(D3D11_Structured_Buffer_Nested, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test reading from structured buffers with nested structs";

  std::string pixel = R"EOSHADER(

struct supernest
{
  float x;
};

struct nest
{
  float3 v;
  supernest s;
  float a, b, c;
};

struct mystruct
{
  nest n[3];
  float4 p;
};

StructuredBuffer<mystruct> buf1 : register(t0);
Buffer<float3> buf2 : register(t1);
RWBuffer<float4> out_buf : register(u1);

float4 main() : SV_Target0
{
  int idx = 0;
  out_buf[idx++] = buf1[0].p;
  out_buf[idx++] = buf1[1].p;
  out_buf[idx++] = buf1[2].p;
  out_buf[idx++] = float4(buf1[0].n[0].v, 1.0f);
  out_buf[idx++] = float4(buf1[3].n[1].v, 1.0f);
  out_buf[idx++] = float4(buf1[6].n[2].v, 1.0f);
  out_buf[idx++] = float4(buf1[4].n[0].a, 0.0f, 0.0f, 1.0f);
  out_buf[idx++] = float4(buf1[5].n[1].b, 0.0f, 0.0f, 1.0f);
  out_buf[idx++] = float4(buf1[7].n[2].c, 0.0f, 0.0f, 1.0f);
  out_buf[idx++] = float4(buf1[8].n[1].s.x, 0.0f, 0.0f, 1.0f);
  idx++;
  out_buf[idx++] = float4(buf2[3], 1.0f);
  out_buf[idx++] = float4(buf2[4], 1.0f);
  out_buf[idx++] = float4(buf2[5], 1.0f);
  return 1.0f.xxxx;
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

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    float data[16 * 100];

    for(int i = 0; i < 16 * 100; i++)
      data[i] = float(i);

    ID3D11BufferPtr structbuf = MakeBuffer().Structured(25 * sizeof(float)).Data(data).SRV();
    ID3D11ShaderResourceViewPtr structbufSRV = MakeSRV(structbuf);

    ID3D11BufferPtr typedbuf = MakeBuffer().Data(data).SRV();
    ID3D11ShaderResourceViewPtr typedbufSRV = MakeSRV(typedbuf).Format(DXGI_FORMAT_R32G32B32_FLOAT);

    ID3D11BufferPtr outbuf = MakeBuffer().Structured(4 * sizeof(float)).Size(1024).UAV();
    ID3D11UnorderedAccessViewPtr outbufUAV = MakeUAV(outbuf);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ID3D11ShaderResourceView *srvs[] = {structbufSRV, typedbufSRV};

      ctx->PSSetShaderResources(0, 2, srvs);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      float zeros[4] = {};
      ctx->ClearUnorderedAccessViewFloat(outbufUAV, zeros);

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &bbRTV.GetInterfacePtr(), NULL, 1, 1,
                                                     &outbufUAV.GetInterfacePtr(), NULL);

      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
