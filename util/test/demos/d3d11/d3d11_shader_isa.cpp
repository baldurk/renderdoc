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

RD_TEST(D3D11_Shader_ISA, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Draws a simple triangle with markers indicating which types of shader ISA we expect to be "
      "available (i.e. running on AMD or not)";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    // make a simple texture so that the structured data includes texture initial states
    ID3D11Texture2DPtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 4).RTV();
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

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      if(adapterDesc.VendorId == PCI_VENDOR_AMD)
        setMarker("GPU=AMD");
      else
        setMarker("GPU=Other");

      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
