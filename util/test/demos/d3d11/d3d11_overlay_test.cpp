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

  std::string depthWritePixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct PixOut
{
	float4 colour : SV_Target0;
	float depth : SV_Depth;
};

PixOut main(v2f IN)
{
  PixOut OUT;
	OUT.colour  = IN.col;
  if ((IN.pos.x > 180.0) && (IN.pos.x < 185.0) &&
      (IN.pos.y > 155.0) && (IN.pos.y < 165.0))
	{
		OUT.depth = 0.0;
	}
	else
	{
		OUT.depth = IN.pos.z;
	}
  return OUT;
}

)EOSHADER";

  std::string discardPixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct PixOut
{
	float4 colour : SV_Target0;
};

PixOut main(v2f IN)
{
  PixOut OUT;
	OUT.colour  = IN.col;
  if ((IN.pos.x > 327.0) && (IN.pos.x < 339.0) &&
      (IN.pos.y > 38.0) && (IN.pos.y < 48.0))
	{
    discard;
	}
  return OUT;
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
    ID3D11PixelShaderPtr depthwriteps = CreatePS(Compile(depthWritePixel, "main", "ps_4_0"));
    ID3D11PixelShaderPtr discardps = CreatePS(Compile(discardPixel, "main", "ps_4_0"));

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

        // fullscreen quad used with scissor to set stencil
        // -1,-1 - +1,-1
        //   |     /
        // -1,+1
        {Vec3f(-1.0f, -1.0f, 0.99f), Vec4f(0.2f, 0.2f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, +1.0f, 0.99f), Vec4f(0.2f, 0.2f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(+1.0f, -1.0f, 0.99f), Vec4f(0.2f, 0.2f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},
        //      +1,-1
        //    /    |
        // -1,+1 - +1,+1
        {Vec3f(+1.0f, -1.0f, 0.99f), Vec4f(0.2f, 0.2f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, +1.0f, 0.99f), Vec4f(0.2f, 0.2f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(+1.0f, +1.0f, 0.99f), Vec4f(0.2f, 0.2f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},

        // discard rectangle
        {Vec3f(0.6f, +0.7f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, +0.9f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, +0.7f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(VBData);

    UINT numQuals = 0;
    dev->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, &numQuals);

    // when there's more than one quality, use a non-zero just to be awkward.
    ID3D11Texture2DPtr msaatex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight)
            .RTV()
            .Multisampled(4, numQuals > 1 ? 1 : 0);

    ID3D11RenderTargetViewPtr msaartv = MakeRTV(msaatex);

    char *fmtNames[] = {"D24_S8", "D32F_S8", "D16_S0", "D32F_S0"};
    DXGI_FORMAT fmts[] = {DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                          DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT};
    const size_t countFmts = ARRAY_COUNT(fmts);

    ID3D11Texture2DPtr depthtexs[countFmts];
    ID3D11DepthStencilViewPtr dsvs[countFmts];
    ID3D11Texture2DPtr msaadepthtexs[countFmts];
    ID3D11DepthStencilViewPtr msaadsvs[countFmts];
    for(size_t f = 0; f < countFmts; f++)
    {
      DXGI_FORMAT fmt = fmts[f];

      ID3D11Texture2DPtr depthtex = MakeTexture(fmt, screenWidth, screenHeight).DSV();
      ID3D11DepthStencilViewPtr dsv = MakeDSV(depthtex);
      depthtexs[f] = depthtex;
      dsvs[f] = dsv;

      dev->CheckMultisampleQualityLevels(fmt, 4, &numQuals);
      ID3D11Texture2DPtr msaadepthtex =
          MakeTexture(fmt, screenWidth, screenHeight).DSV().Multisampled(4, numQuals > 1 ? 1 : 0);
      ID3D11DepthStencilViewPtr msaadsv = MakeDSV(msaadepthtex);
      msaadepthtexs[f] = msaadepthtex;
      msaadsvs[f] = msaadsv;
    }

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

      for(size_t f = 0; f < countFmts; f++)
      {
        std::string fmtName(fmtNames[f]);
        DXGI_FORMAT fmt = fmts[f];
        bool hasStencil = false;
        if(fmt == DXGI_FORMAT_D24_UNORM_S8_UINT || fmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
          hasStencil = true;
        for(ID3D11RenderTargetViewPtr rtv : {bbRTV, msaartv})
        {
          ctx->PSSetShader(ps, NULL, 0);
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

          ID3D11DepthStencilViewPtr curDSV = rtv == msaartv ? msaadsvs[f] : dsvs[f];

          ctx->OMSetRenderTargets(1, &rtv.GetInterfacePtr(), curDSV);

          ClearRenderTargetView(rtv, {0.2f, 0.2f, 0.2f, 1.0f});
          ctx->ClearDepthStencilView(curDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

          if(hasStencil)
          {
            SetStencilRef(0x1);
            depth.StencilEnable = TRUE;
            SetDepthState(depth);
            RSSetScissor({32, 32, 38, 38});
            ctx->Draw(6, 36);
            RSSetScissor({0, 0, screenWidth, screenHeight});
            SetStencilRef(0x55);
            depth.StencilEnable = FALSE;
          }

          // draw the setup triangles

          // 1: write depth
          depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
          SetDepthState(depth);
          ctx->Draw(3, 0);

          // 2: write stencil
          depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
          if(hasStencil)
          {
            depth.StencilEnable = TRUE;
            SetDepthState(depth);
            ctx->Draw(3, 3);
          }

          // 3: write background
          depth.StencilEnable = FALSE;
          SetDepthState(depth);
          ctx->Draw(3, 6);

          // add a marker so we can easily locate this draw
          std::string markerName(rtv == msaartv ? "MSAA Test " : "Normal Test ");
          markerName += fmtName;
          setMarker(markerName);

          depth.StencilEnable = TRUE;
          depth.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
          SetDepthState(depth);
          ctx->PSSetShader(depthwriteps, NULL, 0);
          ctx->Draw(24, 9);

          markerName = "Discard " + markerName;
          setMarker(markerName);
          ctx->PSSetShader(discardps, NULL, 0);
          ctx->Draw(3, 42);
          ctx->PSSetShader(ps, NULL, 0);

          depth.StencilEnable = FALSE;
          depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
          SetDepthState(depth);

          if(rtv == bbRTV)
          {
            setMarker("Viewport Test " + fmtName);
            RSSetViewport({10.0f, 10.0f, 80.0f, 80.0f, 0.0f, 1.0f});
            RSSetScissor({24, 24, 76, 76});
            ctx->Draw(3, 33);
          }

          if(rtv == msaartv)
          {
            setMarker("Sample Mask Test " + fmtName);
            RSSetViewport({0.0f, 0.0f, 80.0f, 80.0f, 0.0f, 1.0f});
            RSSetScissor({0, 0, 80, 80});
            ctx->OMSetBlendState(NULL, NULL, 0x2);
            ctx->Draw(3, 6);
            ctx->OMSetBlendState(NULL, NULL, ~0U);
          }
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
