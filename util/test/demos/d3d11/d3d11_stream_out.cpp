/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

TEST(D3D11_Stream_Out, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Test using D3D11's streamout feature";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    std::vector<D3D11_SO_DECLARATION_ENTRY> sodecl;
    std::vector<UINT> strides;

    {
      D3D11_SO_DECLARATION_ENTRY decl;
      decl.StartComponent = 0;
      decl.ComponentCount = 4;
      decl.SemanticName = "SV_POSITION";
      decl.SemanticIndex = 0;

      decl.Stream = 0;
      decl.OutputSlot = 0;
      sodecl.push_back(decl);
    }
    {
      D3D11_SO_DECLARATION_ENTRY decl;
      decl.StartComponent = 0;
      decl.ComponentCount = 4;
      decl.SemanticName = "COLOR";
      decl.SemanticIndex = 0;

      decl.Stream = 0;
      decl.OutputSlot = 1;
      sodecl.push_back(decl);
    }
    strides.push_back(4 * sizeof(float));
    strides.push_back(8 * sizeof(float));

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);
    ID3D11GeometryShaderPtr gs = CreateGS(vsblob, sodecl, strides);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    ID3D11BufferPtr so[2] = {
        MakeBuffer().StreamOut().Vertex().Size(2048), MakeBuffer().StreamOut().Vertex().Size(2048),
    };

    D3D11_INPUT_ELEMENT_DESC layoutdesc[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
        {
            "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
    };

    ID3D11InputLayoutPtr streamoutLayout;
    CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                    vsblob->GetBufferSize(), &streamoutLayout));

    while(Running())
    {
      ctx->ClearState();

      unsigned char empty[2048] = {};
      ctx->UpdateSubresource(so[0], 0, NULL, empty, 2048, 2048);
      ctx->UpdateSubresource(so[1], 0, NULL, empty, 2048, 2048);

      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->GSSetShader(gs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      ID3D11Buffer *bufs[] = {so[0], so[1]};
      UINT offs[2] = {0};
      ctx->SOSetTargets(2, bufs, offs);

      ctx->Draw(3, 0);

      ctx->UpdateSubresource(so[0], 0, NULL, empty, 2048, 2048);
      ctx->UpdateSubresource(so[1], 0, NULL, empty, 2048, 2048);

      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      ctx->Draw(3, 0);

      ctx->UpdateSubresource(so[0], 0, NULL, empty, 2048, 2048);
      ctx->UpdateSubresource(so[1], 0, NULL, empty, 2048, 2048);

      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      // test using offsets of NULL. Should be equivalent to passing -1
      bufs[0] = so[1];
      bufs[1] = so[0];
      ctx->SOSetTargets(2, bufs, NULL);

      ctx->Draw(3, 0);

      ctx->UpdateSubresource(so[0], 0, NULL, empty, 2048, 2048);
      ctx->UpdateSubresource(so[1], 0, NULL, empty, 2048, 2048);

      // test DrawAuto()

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      // draw with streamout and explicitly unbind
      ctx->SOSetTargets(2, bufs, offs);
      ctx->Draw(3, 0);
      ctx->SOSetTargets(0, NULL, NULL);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth / 4.0f, (float)screenHeight / 4.0f, 0.0f, 1.0f});

      ctx->IASetVertexBuffers(0, 2, bufs, &strides[0], offs);
      ctx->IASetInputLayout(streamoutLayout);
      ctx->DrawAuto();

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ID3D11Buffer *emptyBuf[2] = {};
      ctx->IASetVertexBuffers(0, 2, emptyBuf, &strides[0], offs);
      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetInputLayout(defaultLayout);

      // draw with streamout and clear state
      ctx->SOSetTargets(2, bufs, offs);
      ctx->DrawInstanced(3, 2, 0, 0);
      ctx->ClearState();

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->GSSetShader(gs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      RSSetViewport({screenWidth / 4.0f, 0.0f, (float)screenWidth / 4.0f,
                     (float)screenHeight / 4.0f, 0.0f, 1.0f});

      ctx->IASetVertexBuffers(0, 2, bufs, &strides[0], offs);
      ctx->IASetInputLayout(streamoutLayout);
      ctx->DrawAuto();

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
