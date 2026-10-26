/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2026 Baldur Karlsson
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

RD_TEST(D3D11_Stripped_Shaders, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Tests shaders with their debug/reflection info stripped out and stored in separate blobs";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    MakeDir((GetCWD() + "/tmp/D3D11_Stripped_Shaders").c_str());

    // several variables:
    //
    // legacy whole unstripped blob, or dxc-style just pdb
    // (for dxc-style) manual named or auto-named
    // (for legacy-style) lz4 compressed or not
    // (for legacy-style) PRIV pointed or SetPrivateData
    // absolute or relative path
    //
    // we don't test all possible combinations but check at least one of each

    // PRIV will be set with absolute path
    ID3DBlobPtr vsblob =
        Compile(D3DDefaultVertex + "\n// shader source // vertex", "main", "vs_5_0",
                CompileOptionFlags::SeparateDebugLegacy | CompileOptionFlags::SeparateDebugSetPRIV);
    // we'll set relative SetPrivateData path below
    ID3DBlobPtr psblob =
        Compile(D3DDefaultPixel + "\n// shader source // pixel", "main", "ps_5_0",
                CompileOptionFlags::SeparateDebugLegacy | CompileOptionFlags::ManualDebugName |
                    CompileOptionFlags::LZ4Compress);

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    SetBlobPath("lz4#shader_ps.dbg", ps);

    // manual name will be set absolute in a manual ILDN
    ID3DBlobPtr vsblob2 =
        Compile(D3DDefaultVertex + "\n// shader source 2 // vertex", "main", "vs_5_0",
                CompileOptionFlags::SeparateDebug | CompileOptionFlags::ManualDebugName);
    // the fxc-generated ILDN will be relative
    ID3DBlobPtr psblob2 = Compile(D3DDefaultPixel + "\n// shader source 2 // pixel", "main",
                                  "ps_5_0", CompileOptionFlags::SeparateDebug);

    ID3D11VertexShaderPtr vs2 = CreateVS(vsblob2);
    ID3D11PixelShaderPtr ps2 = CreatePS(psblob2);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      setMarker("test: shader source");
      setMarker("vertex auto absolute");
      setMarker("pixel manual relative");
      ctx->Draw(3, 0);

      ctx->VSSetShader(vs2, NULL, 0);
      ctx->PSSetShader(ps2, NULL, 0);

      setMarker("test: shader source 2");
      setMarker("vertex manual absolute");
      setMarker("pixel auto relative");
      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
