/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

RD_TEST(D3D11_Overlay_Test, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Makes a couple of draws that show off all the overlays in some way";

  std::string whitePixel = R"EOSHADER(

float4 main() : SV_Target0
{
	return float4(1, 1, 1, 1);
}

)EOSHADER";

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
    ID3D11PixelShaderPtr whiteps = CreatePS(Compile(whitePixel, "main", "ps_4_0"));

    const DefaultA2V VBData[] = {
        // this triangle occludes in depth
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle occludes in stencil
        {Vec3f(-0.5f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.5f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle is just in the background to contribute to overdraw
        {Vec3f(-0.9f, -0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.9f, -0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(1.0f, 0.0f)},

        // the draw has a few triangles, main one that is occluded for depth, another that is
        // adding to overdraw complexity, one that is backface culled, then a few more of various
        // sizes for triangle size overlay
        {Vec3f(-0.3f, -0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.2f, -0.2f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.2f, 0.0f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.2f, -0.4f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // backface culled
        {Vec3f(0.1f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, -0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // depth clipped (i.e. not clamped)
        {Vec3f(0.6f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.0f, 1.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // small triangles
        // size=0.01
        {Vec3f(0.0f, 0.4f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.41f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.01f, 0.4f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.015
        {Vec3f(0.0f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.515f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.015f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.02
        {Vec3f(0.0f, 0.6f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.62f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.02f, 0.6f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.025
        {Vec3f(0.0f, 0.7f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.725f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.025f, 0.7f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle deliberately goes out of the viewport, it will test viewport & scissor
        // clipping
        {Vec3f(-1.3f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.3f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(VBData);

    ID3D11Texture2DPtr depthtex =
        MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight).DSV();
    ID3D11DepthStencilViewPtr dsv = MakeDSV(depthtex);

    UINT numQuals = 0;
    dev->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, &numQuals);

    // when there's more than one quality, use a non-zero just to be awkward.
    ID3D11Texture2DPtr msaatex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight)
            .RTV()
            .Multisampled(4, numQuals > 1 ? 1 : 0);
    ID3D11RenderTargetViewPtr msaartv = MakeRTV(msaatex);

    dev->CheckMultisampleQualityLevels(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 4, &numQuals);
    ID3D11Texture2DPtr msaadepthtex =
        MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight)
            .DSV()
            .Multisampled(4, numQuals > 1 ? 1 : 0);
    ID3D11DepthStencilViewPtr msaadsv = MakeDSV(msaadepthtex);

    ID3D11Texture2DPtr subtex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight).RTV().Array(5).Mips(4);
    ID3D11RenderTargetViewPtr subrtv =
        MakeRTV(subtex).FirstSlice(2).NumSlices(1).FirstMip(2).NumMips(1);
    ID3D11RenderTargetViewPtr subrtv2 =
        MakeRTV(subtex).FirstSlice(2).NumSlices(1).FirstMip(3).NumMips(1);

    while(Running())
    {
      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      for(ID3D11RenderTargetViewPtr rtv : {bbRTV, msaartv})
      {
        D3D11_DEPTH_STENCIL_DESC depth = GetDepthState();

        depth.StencilEnable = FALSE;
        depth.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depth.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;

        SetDepthState(depth);
        SetStencilRef(0x55);

        D3D11_RASTERIZER_DESC raster = GetRasterState();

        raster.ScissorEnable = TRUE;

        SetRasterState(raster);

        RSSetViewport(
            {10.0f, 10.0f, (float)screenWidth - 20.0f, (float)screenHeight - 20.0f, 0.0f, 1.0f});
        RSSetScissor({0, 0, screenWidth, screenHeight});

        ID3D11DepthStencilViewPtr curDSV = rtv == msaartv ? msaadsv : dsv;

        ctx->OMSetRenderTargets(1, &rtv.GetInterfacePtr(), curDSV);

        ClearRenderTargetView(rtv, {0.2f, 0.2f, 0.2f, 1.0f});
        ctx->ClearDepthStencilView(curDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // draw the setup triangles

        // 1: write depth
        depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
        SetDepthState(depth);
        ctx->Draw(3, 0);

        // 2: write stencil
        depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depth.StencilEnable = TRUE;
        SetDepthState(depth);
        ctx->Draw(3, 3);

        // 3: write background
        depth.StencilEnable = FALSE;
        SetDepthState(depth);
        ctx->Draw(3, 6);

        // add a marker so we can easily locate this draw
        setMarker(rtv == msaartv ? "MSAA Test" : "Normal Test");

        depth.StencilEnable = TRUE;
        depth.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
        SetDepthState(depth);
        ctx->Draw(24, 9);

        depth.StencilEnable = FALSE;
        depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
        SetDepthState(depth);

        if(rtv == bbRTV)
        {
          setMarker("Viewport Test");
          RSSetViewport({10.0f, 10.0f, 80.0f, 80.0f, 0.0f, 1.0f});
          RSSetScissor({24, 24, 76, 76});
          ctx->Draw(3, 33);
        }
      }

      ctx->PSSetShader(whiteps, NULL, 0);

      RSSetViewport({5.0f, 5.0f, float(screenWidth) / 4.0f - 10.0f,
                     float(screenHeight) / 4.0f - 10.0f, 0.0f, 1.0f});
      RSSetScissor({0, 0, screenWidth / 4, screenHeight / 4});

      ClearRenderTargetView(subrtv, {0.0f, 0.0f, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &subrtv.GetInterfacePtr(), NULL);
      setMarker("Subresources mip 2");
      ctx->Draw(24, 9);

      RSSetViewport(
          {2.0f, 2.0f, float(screenWidth / 8) - 4.0f, float(screenHeight / 8) - 4.0f, 0.0f, 1.0f});
      RSSetScissor({0, 0, screenWidth / 8, screenHeight / 8});

      ClearRenderTargetView(subrtv2, {0.0f, 0.0f, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &subrtv2.GetInterfacePtr(), NULL);
      setMarker("Subresources mip 3");
      ctx->Draw(24, 9);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
