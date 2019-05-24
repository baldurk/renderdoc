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

TEST(D3D11_Deferred_UpdateSubresource, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test that does UpdateSubresource on a deferred context which might need some "
      "workaround code.";

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

    ID3D11DeviceContextPtr defctx;
    dev->CreateDeferredContext(0, &defctx);

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    DefaultA2V triangle[] = {
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},

        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 2.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(2.0f, 0.0f)},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(triangle);

    ID3D11Texture2DPtr tex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).SRV();
    ID3D11ShaderResourceViewPtr srv = MakeSRV(tex);

    ID3D11Texture2DPtr tex2 = MakeTexture(DXGI_FORMAT_R8_UNORM, 2048, 2048).SRV();
    ID3D11ShaderResourceViewPtr srv2 = MakeSRV(tex2);

    float *buffers[3];

    // each buffer is twice the size of the texture so we can see any reads from
    // before the source area
    for(size_t i = 0; i < 3; i++)
      buffers[i] = new float[64 * 64 * 4 * 2];

    for(size_t i = 0; i < 64 * 64 * 2; i++)
    {
      // first buffer is dark grey
      buffers[0][i * 4 + 0] = 0.1f;
      buffers[0][i * 4 + 1] = 0.1f;
      buffers[0][i * 4 + 2] = 0.1f;
      buffers[0][i * 4 + 3] = 1.0f;

      // others have red to mark 'incorrect' areas.
      // should never be read from
      for(size_t x = 1; x < 3; x++)
      {
        buffers[x][i * 4 + 0] = 1.0f;
        buffers[x][i * 4 + 1] = 0.0f;
        buffers[x][i * 4 + 2] = 0.0f;
        buffers[x][i * 4 + 3] = 1.0f;
      }
    }

    float *srcArea[4];
    for(size_t i = 0; i < 3; i++)
      srcArea[i] = buffers[i] + 64 * 64 * 4;

    for(size_t i = 0; i < 16 * 16; i++)
    {
      // fill first buffer with random green colours
      srcArea[1][i * 4 + 0] = 0.2f;
      srcArea[1][i * 4 + 1] = RANDF(0.0f, 1.0f);
      srcArea[1][i * 4 + 2] = 0.2f;
      srcArea[1][i * 4 + 3] = 1.0f;

      // second with random blue colours
      srcArea[2][i * 4 + 0] = 0.2f;
      srcArea[2][i * 4 + 1] = 0.2f;
      srcArea[2][i * 4 + 2] = RANDF(0.0f, 1.0f);
      srcArea[2][i * 4 + 3] = 1.0f;
    }

    D3D11_BOX leftBox = {4, 4, 0, 20, 20, 1};
    D3D11_BOX toprightBox = {44, 44, 0, 60, 60, 1};
    D3D11_BOX botrightBox = {44, 4, 0, 60, 20, 1};

    // corrected deferred context version of srcArea[2]
    srcArea[3] = srcArea[2];
    // pAdjustedSrcData = ((const BYTE*)pSrcData) - (alignedBox.front * srcDepthPitch) -
    // (alignedBox.top * srcRowPitch) - (alignedBox.left * srcBytesPerElement);

    D3D11_FEATURE_DATA_THREADING threadingCaps = {FALSE, FALSE};

    HRESULT hr =
        dev->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadingCaps, sizeof(threadingCaps));
    if(SUCCEEDED(hr))
    {
      if(!threadingCaps.DriverCommandLists)
      {
        srcArea[3] = (float *)(((BYTE *)srcArea[3]) - (botrightBox.top * 16 * sizeof(float) * 4) -
                               (botrightBox.left * sizeof(float) * 4));
      }
    }

    ID3D11CommandListPtr cmdList;

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      // first clear the texture with no box, fully black, on the immediate context
      ctx->UpdateSubresource(tex, 0, NULL, srcArea[0], 64 * sizeof(float) * 4,
                             64 * 64 * sizeof(float) * 4);

      ctx->Draw(3, 0);

      // now write some random green bits into a left box, on the immediate context
      ctx->UpdateSubresource(tex, 0, &leftBox, srcArea[1], 16 * sizeof(float) * 4,
                             16 * 16 * sizeof(float) * 4);

      ctx->Draw(3, 3);

      // now write some random blue bits into a left box, on the deferred context, WITHOUT
      // correction
      defctx->UpdateSubresource(tex, 0, &toprightBox, srcArea[2], 16 * sizeof(float) * 4,
                                16 * 16 * sizeof(float) * 4);

      defctx->FinishCommandList(TRUE, &cmdList);

      ctx->ExecuteCommandList(cmdList, TRUE);

      ctx->Draw(3, 6);

      cmdList = NULL;

      // now write some random blue bits into a left box, on the deferred context, WITH correction
      defctx->UpdateSubresource(tex, 0, &botrightBox, srcArea[3], 16 * sizeof(float) * 4,
                                16 * 16 * sizeof(float) * 4);

      defctx->FinishCommandList(TRUE, &cmdList);

      ctx->ExecuteCommandList(cmdList, TRUE);

      ctx->Draw(3, 9);

      // test update with box to ensure we don't read too much data
      D3D11_BOX smallbox = {2000, 2000, 0, 2040, 2040, 1};
      byte *smalldata = new byte[2048 * 39 + 40];
      memset(smalldata, 0xfd, 2048 * 39 + 40);
      ctx->UpdateSubresource(tex2, 0, &smallbox, smalldata, 2048, 0);

      delete[] smalldata;

      cmdList = NULL;

      Present();
    }

    for(size_t i = 0; i < 3; i++)
      delete[] buffers[i];

    return 0;
  }
};

REGISTER_TEST();
