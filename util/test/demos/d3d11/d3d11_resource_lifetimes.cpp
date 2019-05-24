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

TEST(D3D11_Resource_Lifetimes, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test various edge-case resource lifetimes: a resource that is first dirtied within a frame "
      "so needs initial contents created for it, and a resource that is created and destroyed "
      "mid-frame (which also gets dirtied after use).";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D smiley : register(t0);
Texture2D checker : register(t1);
SamplerState samp : register(s0);

cbuffer consts : register(b0)
{
  float4 flags;
};

float4 main(v2f IN) : SV_Target0
{
  if(flags.x != 1.0f || flags.y != 2.0f || flags.z != 4.0f || flags.w != 8.0f)
    return float4(1.0f, 0.0f, 1.0f, 1.0f);

	return smiley.Sample(samp, IN.uv * 2.0f) * checker.Sample(samp, IN.uv * 5.0f);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    ID3D11Texture2DPtr smiley =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, rgba8.width, rgba8.height).SRV();
    ID3D11ShaderResourceViewPtr smileysrv = MakeSRV(smiley);

    ctx->UpdateSubresource(smiley, 0, NULL, rgba8.data.data(), rgba8.width * sizeof(uint32_t), 0);

    ID3D11SamplerStatePtr samp = MakeSampler().Address(D3D11_TEXTURE_ADDRESS_WRAP);

    auto SetupBuf = [this]() {
      const Vec4f flags = {1.0f, 2.0f, 4.0f, 8.0f};

      ID3D11BufferPtr ret = MakeBuffer().Constant().Size(sizeof(flags)).Mappable();

      D3D11_MAPPED_SUBRESOURCE map = Map(ret, 0, D3D11_MAP_WRITE_DISCARD);
      memcpy(map.pData, &flags, sizeof(Vec4f));
      ctx->Unmap(ret, 0);

      return ret;
    };

    auto TrashBuf = [this](ID3D11BufferPtr &buf) {
      D3D11_MAPPED_SUBRESOURCE map = Map(buf, 0, D3D11_MAP_WRITE_DISCARD);
      memset(map.pData, 0, sizeof(Vec4f));
      ctx->Unmap(buf, 0);

      buf = NULL;
    };

    auto SetupSRV = [this]() {
      ID3D11Texture2DPtr tex = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4).SRV();
      ID3D11ShaderResourceViewPtr ret = MakeSRV(tex);

      const uint32_t checker[4 * 4] = {
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
      };

      ctx->UpdateSubresource(tex, 0, NULL, checker, 4 * sizeof(uint32_t), 0);

      return ret;
    };

    auto TrashSRV = [this](ID3D11ShaderResourceViewPtr &srv) {
      ID3D11Resource *res = NULL;
      srv->GetResource(&res);

      ID3D11Texture2DPtr tex = res;
      res->Release();

      const uint32_t empty[4 * 4] = {};

      ctx->UpdateSubresource(tex, 0, NULL, empty, 4 * sizeof(uint32_t), 0);

      srv = NULL;
    };

    ID3D11ShaderResourceView *srvs[2] = {smileysrv};

    ctx->PSSetSamplers(0, 1, &samp.GetInterfacePtr());

    ID3D11BufferPtr cb = SetupBuf();
    ID3D11ShaderResourceViewPtr srv = SetupSRV();
    srvs[1] = srv;
    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);
      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      // render with last frame's resources
      RSSetViewport({0.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f});
      ctx->Draw(3, 0);

      TrashBuf(cb);
      TrashSRV(srv);

      // create resources mid-frame and use then trash them
      cb = SetupBuf();
      srv = SetupSRV();
      srvs[1] = srv;
      ctx->PSSetShaderResources(0, 2, srvs);
      ctx->PSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());
      RSSetViewport({128.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f});
      ctx->Draw(3, 0);
      TrashBuf(cb);
      TrashSRV(srv);

      // set up resources for next frame
      cb = SetupBuf();
      srv = SetupSRV();

      srvs[1] = srv;
      ctx->PSSetShaderResources(0, 2, srvs);
      ctx->PSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
