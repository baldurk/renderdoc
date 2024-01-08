/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

RD_TEST(D3D11_Parameter_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "General tests of parameters known to cause problems - e.g. optional values that should be "
      "ignored, edge cases, special values, etc.";

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

    ID3DDeviceContextStatePtr ctxstate, ctxstate_off;

    D3D_FEATURE_LEVEL feat11 = D3D_FEATURE_LEVEL_11_0;
    CHECK_HR(dev1->CreateDeviceContextState(0, &feat11, 1, D3D11_SDK_VERSION,
                                            __uuidof(ID3D11Device), NULL, &ctxstate));
    CHECK_HR(dev1->CreateDeviceContextState(0, &feat11, 1, D3D11_SDK_VERSION,
                                            __uuidof(ID3D11Device), NULL, &ctxstate_off));

    ctx1->SwapDeviceContextState(ctxstate_off, NULL);

    std::string features1_tiled_resources("Features1: D3D11_TILED_RESOURCES_SUPPORTED");
    std::string features2_tiled_resources("Features2: D3D11_TILED_RESOURCES_SUPPORTED");
    std::string create_tiled_buffer("CreateTiledBuffer: Passed");
    std::string create_tile_pool_buffer("CreateTile_PoolBuffer: Passed");
    std::string create_tiled_texture2D("CreateTiledTexture2D: Passed");
    std::string create_tiled_texture2D1("CreateTiledTexture2D1: Passed");
    if(dev2)
    {
      D3D11_FEATURE_DATA_D3D11_OPTIONS1 features1;
      CHECK_HR(dev2->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS1, &features1, sizeof(features1)));
      if(features1.TiledResourcesTier == D3D11_TILED_RESOURCES_NOT_SUPPORTED)
        features1_tiled_resources = "Features1: D3D11_TILED_RESOURCES_NOT_SUPPORTED";

      D3D11_FEATURE_DATA_D3D11_OPTIONS2 features2;
      CHECK_HR(dev2->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &features2, sizeof(features2)));
      if(features2.TiledResourcesTier == D3D11_TILED_RESOURCES_NOT_SUPPORTED)
        features2_tiled_resources = "Features2: D3D11_TILED_RESOURCES_NOT_SUPPORTED";

      // Check trying to create tiled resources fails
      D3D11_BUFFER_DESC bufDesc = {};
      bufDesc.ByteWidth = 1024;
      bufDesc.Usage = D3D11_USAGE_DEFAULT;
      bufDesc.StructureByteStride = 1;
      bufDesc.CPUAccessFlags = 0;
      bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      bufDesc.MiscFlags = D3D11_RESOURCE_MISC_TILED;
      if(FAILED(dev2->CreateBuffer(&bufDesc, NULL, NULL)))
        create_tiled_buffer = "CreateTiledBuffer: Failed";

      bufDesc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;
      if(FAILED(dev2->CreateBuffer(&bufDesc, NULL, NULL)))
        create_tile_pool_buffer = "CreateTile_PoolBuffer: Failed";

      D3D11_TEXTURE2D_DESC texDesc = {};
      texDesc.Width = 8;
      texDesc.Height = 8;
      texDesc.MipLevels = 1;
      texDesc.ArraySize = 1;
      texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      texDesc.SampleDesc.Count = 1;
      texDesc.SampleDesc.Quality = 0;
      texDesc.Usage = D3D11_USAGE_DEFAULT;
      texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      texDesc.CPUAccessFlags = 0;
      texDesc.MiscFlags = D3D11_RESOURCE_MISC_TILED;
      if(FAILED(dev2->CreateTexture2D(&texDesc, NULL, NULL)))
        create_tiled_texture2D = "CreateTiledTexture2D: Failed";
    }
    if(dev3)
    {
      D3D11_TEXTURE2D_DESC1 texDesc = {};
      texDesc.Width = 8;
      texDesc.Height = 8;
      texDesc.MipLevels = 1;
      texDesc.ArraySize = 1;
      texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      texDesc.SampleDesc.Count = 1;
      texDesc.SampleDesc.Quality = 0;
      texDesc.Usage = D3D11_USAGE_DEFAULT;
      texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      texDesc.CPUAccessFlags = 0;
      texDesc.MiscFlags = D3D11_RESOURCE_MISC_TILED;
      if(FAILED(dev3->CreateTexture2D1(&texDesc, NULL, NULL)))
        create_tiled_texture2D1 = "CreateTiledTexture2D1: Failed";
    }

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});

      // set the ctxstate, so it only exists in the context's memory (which we don't track)
      ctx1->SwapDeviceContextState(ctxstate, NULL);
      // release our resource, renderdoc will destroy it now
      ctxstate = NULL;

      // repeatedly toggle between the states and re-destroy ctxstate
      for(int i = 0; i < 100; i++)
      {
        // we always need an incoming state, pass it in and get back our old state that we destroyed
        ctx1->SwapDeviceContextState(ctxstate_off, &ctxstate);

        // again make it disappear
        ctx1->SwapDeviceContextState(ctxstate, NULL);
        ctxstate = NULL;
      }

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissor({0, 0, 1, 1});

      D3D11_RASTERIZER_DESC raster = GetRasterState();
      raster.ScissorEnable = FALSE;
      SetRasterState(raster);

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      ctx->Draw(3, 0);

      setMarker(features1_tiled_resources);
      setMarker(features2_tiled_resources);
      setMarker(create_tiled_buffer);
      setMarker(create_tile_pool_buffer);
      setMarker(create_tiled_texture2D);
      setMarker(create_tiled_texture2D1);

      Present();

      // get back to how we should be with a handle to ctxstate and ctxstate_off bound
      ctx1->SwapDeviceContextState(ctxstate_off, &ctxstate);
    }

    return 0;
  }
};

REGISTER_TEST();
