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

#include "../win32/win32_window.h"
#include "d3d11_test.h"

RD_TEST(D3D11_Swapchain_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests all types of swapchain that D3D11 supports.";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    DXGI_SWAP_CHAIN_DESC swapDesc;
    ZeroMemory(&swapDesc, sizeof(swapDesc));

    swapDesc.BufferCount = backbufferCount;
    swapDesc.BufferDesc.Width = screenWidth;
    swapDesc.BufferDesc.Height = screenHeight;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.Windowed = TRUE;
    swapDesc.Flags = 0;

    std::vector<Win32Window *> wins;
    std::vector<IDXGISwapChainPtr> swaps;
    std::vector<ID3D11RenderTargetViewPtr> bbs;
    std::vector<std::string> names;

    auto addWindow = [&](std::string name) {
      wins.push_back(new Win32Window(screenWidth, screenHeight, screenTitle));
      swaps.push_back({});
      bbs.push_back({});
      names.push_back(name);
      swapDesc.OutputWindow = wins.back()->wnd;
      fact->CreateSwapChain(dev, &swapDesc, &swaps.back());

      ID3D11Texture2DPtr bb;
      CHECK_HR(swaps.back()->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb));

      CHECK_HR(dev->CreateRenderTargetView(bb, NULL, &bbs.back()));
    };

    // always support sequential flip
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    addWindow("SEQUENTIAL");

    // ditto MSAA
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 4;
    addWindow("MSAA RGBA8");

    // ditto FP16 (on feature level 10 and above)
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    swapDesc.SampleDesc.Count = 1;
    addWindow("RGBA16");

    // check FP16 and MSAA
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    swapDesc.SampleDesc.Count = 4;
    addWindow("MSAA RGBA16");

    IDXGIFactory4Ptr factory4 = fact;
    if(factory4)
    {
      // only test flip effects if we can get factory4, which is windows 10 only.

      swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      swapDesc.SampleDesc.Count = 1;
      addWindow("FLIP_DISCARD");

      swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      swapDesc.SampleDesc.Count = 1;
      addWindow("FLIP_SEQUENTIAL");
    }

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    while(Running())
    {
      for(Win32Window *win : wins)
        win->Update();

      ClearRenderTargetView(bbRTV, {0.0f, 0.0f, 0.0f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      ctx->Draw(3, 0);

      for(size_t i = 0; i < bbs.size(); i++)
      {
        ID3D11RenderTargetViewPtr bb = bbs[i];
        setMarker(names[i]);
        ctx->OMSetRenderTargets(1, &bb.GetInterfacePtr(), NULL);
        ClearRenderTargetView(bb, {0.0f, 0.0f, 0.0f, 1.0f});
        ctx->Draw(3, 0);
      }

      Present();
      for(IDXGISwapChainPtr s : swaps)
        s->Present(0, 0);
    }

    return 0;
  }
};

REGISTER_TEST();
