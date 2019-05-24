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

TEST(D3D11_Refcount_Check, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that the device etc doesn't delete itself when there are still outstanding "
      "references, and also that it *does* delete itself when any cycle is detected.";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11Debug *dbg = NULL;
    dev.QueryInterface(__uuidof(ID3D11Debug), &dbg);

    dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

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

      ctx->Draw(3, 0);

      Present();
    }

    // remove our references to everything but vb which we take locally
    defaultLayout = NULL;
    vs = NULL;
    ps = NULL;
    swap = NULL;
    bbTex = NULL;
    bbRTV = NULL;
    dev1 = NULL;
    dev2 = NULL;
    ctx = NULL;
    ctx1 = NULL;
    ctx2 = NULL;
    annot = NULL;

    ID3D11Buffer *localvb = vb;
    localvb->AddRef();
    vb = NULL;

    ID3D11Device *localdev = dev;
    localdev->AddRef();
    dev = NULL;

    dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

    ULONG ret = localvb->Release();

    // this should release the VB
    TEST_ASSERT(ret == 0, "localvb still has outstanding references");

    GET_REFCOUNT(ret, localdev);

    // the device should only have at most 2 references - localdev and dbg
    TEST_ASSERT(ret <= 2, "device has too many references");

    dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

    dbg->Release();
    dbg = NULL;

    GET_REFCOUNT(ret, localdev);

    // the device should only have this reference - localdev
    TEST_ASSERT(ret == 1, "device has too many references");

    ret = localdev->Release();

    TEST_ASSERT(ret == 0, "localdev still has outstanding references");

    return 0;
  }
};

REGISTER_TEST();
