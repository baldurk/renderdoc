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

RD_TEST(D3D11_Shader_Editing, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that shader editing works with different combinations of shader re-use.";

  std::string vertex = R"EOSHADER(

float4 main(float3 INpos : POSITION) : SV_Position
{
	float4 ret = float4(0,0,0,1);
  ret.xyz += INpos.xyz;
  return ret;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

float4 main() : SV_Target0
{
#if 1
	return float4(0.0, 1.0, 0.0, 1.0);
#else
	return float4(0.0, 1.0, 1.0, 1.0);
#endif
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    // compile again so that we can edit this one distinctly
    ID3D11PixelShaderPtr ps2 = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    ID3D11Texture2DPtr fltTex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight).RTV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->OMSetRenderTargets(1, &fltRT.GetInterfacePtr(), NULL);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth / 2.0f, (float)screenHeight, 0.0f, 1.0f});
      setMarker("Draw 1");
      ctx->Draw(3, 0);

      ctx->PSSetShader(ps2, NULL, 0);

      RSSetViewport({(float)screenWidth / 2.0f, 0.0f, (float)screenWidth / 2.0f,
                     (float)screenHeight, 0.0f, 1.0f});
      setMarker("Draw 2");
      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
