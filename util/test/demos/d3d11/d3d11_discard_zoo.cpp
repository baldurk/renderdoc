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

RD_TEST(D3D11_Discard_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests texture discarding resources in D3D11.";

  byte empty[16 * 1024 * 1024] = {};

  void Clear(ID3D11Texture2DPtr t)
  {
    if(!t)
      return;

    D3D11_TEXTURE2D_DESC desc = {};
    t->GetDesc(&desc);

    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
    {
      ID3D11RenderTargetViewPtr rt;

      for(UINT m = 0; m < desc.MipLevels; m++)
      {
        rt = MakeRTV(t).FirstMip(m);
        ClearRenderTargetView(rt, {0.0f, 1.0f, 0.0f, 1.0f});
      }
    }
    else if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
    {
      ID3D11DepthStencilViewPtr dsv;

      for(UINT m = 0; m < desc.MipLevels; m++)
      {
        dsv = MakeDSV(t).FirstMip(m);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.4f, 0x40);
      }
    }
    else
    {
      for(UINT i = 0; i < desc.ArraySize * desc.MipLevels; i++)
        ctx->UpdateSubresource(t, i, NULL, empty, 32, 32);
    }
  }

  template <typename T>
  void DiscardView1(T view, UINT x, UINT y, UINT width, UINT height)
  {
    D3D11_RECT rect = {(LONG)x, (LONG)y, LONG(x + width), LONG(y + height)};
    ctx1->DiscardView1(view, &rect, 1);
  }

  template <typename T>
  void DiscardView(T view)
  {
    ctx1->DiscardView(view);
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    memset(empty, 0x88, sizeof(empty));

    std::vector<ID3D11Texture2DPtr> texs;

#define TEX_TEST(name, x)                                                          \
  if(first)                                                                        \
  {                                                                                \
    texs.push_back(x);                                                             \
    Clear(texs.back());                                                            \
    SetDebugName(texs.back(), "Tex" + std::to_string(texs.size()) + ": " + +name); \
  }                                                                                \
  tex = texs[t++];

    ID3D11BufferPtr rtvbuf = MakeBuffer().Size(1024).RTV();
    ID3D11BufferPtr srvbuf = MakeBuffer().Size(1024).SRV();
    ID3D11BufferPtr buf = MakeBuffer().Size(1024).Vertex();
    ID3D11BufferPtr stagingBuf = MakeBuffer().Size(1022).Staging();
    ID3D11BufferPtr dynamicBuf = MakeBuffer().Size(1026).Vertex().Mappable();

    SetDebugName(buf, "Buffer");
    SetDebugName(dynamicBuf, "Buffer Staging");
    SetDebugName(stagingBuf, "Buffer Dynamic");
    SetDebugName(srvbuf, "BufferSRV");
    SetDebugName(rtvbuf, "BufferRTV");

    ID3D11Texture1DPtr tex1d = MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300).Array(5).Mips(3);
    ID3D11Texture3DPtr tex3d = MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300, 15).Mips(3);
    ID3D11Texture1DPtr tex1drtv =
        MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300).Array(5).Mips(3).RTV();
    ID3D11Texture3DPtr tex3drtv =
        MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300, 15).Mips(3).RTV();

    SetDebugName(tex1d, "Tex1D: DiscardAll");
    SetDebugName(tex3d, "Tex3D: DiscardAll");
    SetDebugName(tex1drtv, "Tex1D: DiscardRect Mip1 Slice1,2");
    SetDebugName(tex3drtv, "Tex3D: DiscardRect Mip1 Slice1,2");

    bool first = true;

    while(Running())
    {
      if(!first)
      {
        pushMarker("Clears");
        for(ID3D11Texture2DPtr t : texs)
          Clear(t);
        ctx->UpdateSubresource(rtvbuf, 0, NULL, empty, 1024, 1024);
        ctx->UpdateSubresource(srvbuf, 0, NULL, empty, 1024, 1024);
        ctx->UpdateSubresource(buf, 0, NULL, empty, 1024, 1024);
        ctx->UpdateSubresource(stagingBuf, 0, NULL, empty, 1024, 1024);
        ctx->UpdateSubresource(dynamicBuf, 0, NULL, empty, 1024, 1024);

        ID3D11RenderTargetViewPtr rt;

        for(UINT m = 0; m < 3; m++)
        {
          rt = MakeRTV(tex1drtv).FirstMip(m);
          ClearRenderTargetView(rt, {0.0f, 1.0f, 0.0f, 1.0f});

          rt = MakeRTV(tex3drtv).FirstMip(m);
          ClearRenderTargetView(rt, {0.0f, 1.0f, 0.0f, 1.0f});

          ctx->UpdateSubresource(tex3d, m, NULL, empty, 32, 64);
          for(UINT s = 0; s < 5; s++)
            ctx->UpdateSubresource(tex1d, s * 3 + m, NULL, empty, 32, 64);
        }
        popMarker();
      }

      // this is an anchor point for us to jump to and observe textures with all cleared contents
      // and no discard patterns
      setMarker("TestStart");
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      int t = 0;
      ID3D11Texture2DPtr tex;

      // test a few different formats
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R10G10B10A2_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R10G10B10A2_UINT, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R9G9B9E5_SHAREDEXP, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC1_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC2_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC3_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC4_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC5_UNORM, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC6H_UF16, 300, 300));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_BC7_UNORM, 300, 300));
      ctx1->DiscardResource(tex);

      // test with different mips/array sizes
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Mips(5));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Array(4));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Array(4).Mips(5));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 30, 5));
      ctx1->DiscardResource(tex);

      // test MSAA textures
      TEX_TEST("DiscardAll",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Multisampled(4).RTV());
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Multisampled(4).Array(5).RTV());
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_UINT, 300, 300).Multisampled(4).Array(5).RTV());
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_SINT, 300, 300).Multisampled(4).Array(5).RTV());
      ctx1->DiscardResource(tex);

      // test depth textures
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300).DSV());
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).DSV());
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D24_UNORM_S8_UINT, 300, 300).DSV());
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300).DSV().Mips(5));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300).DSV().Array(4));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300).DSV().Array(4).Mips(5));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).DSV().Mips(5));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).DSV().Array(4));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll",
               MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).DSV().Array(4).Mips(5));
      ctx1->DiscardResource(tex);
      TEX_TEST("DiscardAll",
               MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).Multisampled(4).DSV());
      ctx1->DiscardResource(tex);
      TEX_TEST(
          "DiscardAll",
          MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).Multisampled(4).Array(5).DSV());
      ctx1->DiscardResource(tex);

      // test discarding rects within a texture using DiscardView1. Only supported on RTVs and DSVs
      TEX_TEST("DiscardRect Mip0", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).RTV());
      DiscardView1<ID3D11RenderTargetViewPtr>(MakeRTV(tex), 50, 50, 75, 75);
      TEX_TEST("DiscardRect Mip1",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Mips(2).RTV());
      DiscardView1<ID3D11RenderTargetViewPtr>(MakeRTV(tex).FirstMip(1), 50, 50, 75, 75);

      TEX_TEST("DiscardRect Mip0", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).DSV());
      DiscardView1<ID3D11DepthStencilViewPtr>(MakeDSV(tex), 50, 50, 75, 75);
      TEX_TEST("DiscardRect Mip1",
               MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300).Mips(2).DSV());
      DiscardView1<ID3D11DepthStencilViewPtr>(MakeDSV(tex).FirstMip(1), 50, 50, 75, 75);

      TEX_TEST("DiscardAll Slice2",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Multisampled(4).Array(5).RTV());
      DiscardView<ID3D11RenderTargetViewPtr>(MakeRTV(tex).FirstSlice(2).NumSlices(1));

      // test with DiscardView1 and NULL rect
      TEX_TEST("DiscardAll Slice2",
               MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Multisampled(4).Array(5).RTV());
      ctx1->DiscardView1((ID3D11RenderTargetViewPtr)MakeRTV(tex).FirstSlice(2).NumSlices(1), NULL, 0);

      // test 1D/3D textures
      ctx1->DiscardResource(tex1d);
      ctx1->DiscardResource(tex3d);

      DiscardView1<ID3D11RenderTargetViewPtr>(
          MakeRTV(tex1drtv).FirstMip(1).FirstSlice(1).NumSlices(2), 50, 0, 75, 1);
      DiscardView1<ID3D11RenderTargetViewPtr>(
          MakeRTV(tex3drtv).FirstMip(1).FirstSlice(1).NumSlices(2), 50, 50, 75, 75);

      ///////////////////////////
      // buffers

      // discard the buffer
      ctx1->DiscardResource(buf);
      ctx1->DiscardResource(stagingBuf);
      ctx1->DiscardResource(dynamicBuf);

      // discard the whole SRV buffer (can't discard a rect)
      DiscardView<ID3D11ShaderResourceViewPtr>(
          MakeSRV(srvbuf).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).NumElements(16));

      // discard part of the RTV buffer with a rect
      DiscardView1<ID3D11RenderTargetViewPtr>(
          MakeRTV(rtvbuf).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).NumElements(16), 50, 0, 75, 1);

      setMarker("TestEnd");
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      first = false;

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
