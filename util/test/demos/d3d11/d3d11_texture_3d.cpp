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

TEST(D3D11_Texture_3D, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Test that creates and samples a 3D texture";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float4 uv : TEXCOORD0;
};

Texture3D<float> tex : register(t0);
SamplerState samp : register(s0);

float4 main(v2f IN) : SV_Target0
{
  float4 ret = 0.0f;
  float mul = 0.5f;
  const float step = 0.5f;

  ret += tex.SampleLevel(samp, IN.uv.yxx, 7.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.xyx, 6.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.xxy, 5.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.yxy, 4.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.yyx, 3.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.xyy, 2.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.yyy, 1.0f) * mul; mul *= step;
  ret += tex.SampleLevel(samp, IN.uv.xxx, 0.0f) * mul;

  return (ret + 0.5f) * IN.col;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11SamplerStatePtr samp = MakeSampler();

    ID3D11Texture3DPtr tex = MakeTexture(DXGI_FORMAT_R8_UNORM, 128, 128, 1024).Mips(8).SRV();

    uint8_t *data = new uint8_t[128 * 128 * 1024 * sizeof(uint8_t)];

    char *digits[10] = {

        "..####.."
        ".#....#."
        "#......#"
        "#......#"    // 0
        "#......#"
        "#......#"
        ".#....#."
        "..####..",

        "....#..."
        "...##..."
        "..#.#..."
        "....#..."    // 1
        "....#..."
        "....#..."
        "....#..."
        "..####..",

        "..###..."
        ".#...#.."
        ".....#.."
        "....#..."    // 2
        "....#..."
        "...#...."
        "...#...."
        "..####..",

        "..###..."
        ".#...#.."
        ".....#.."
        ".....#.."    // 3
        "..###..."
        ".....#.."
        ".#...#.."
        "..###...",

        "........"
        "....#..."
        "...#...."
        "..#....."
        ".#..#..."    // 4
        ".#####.."
        "....#..."
        "....#...",

        ".#####.."
        ".#......"
        ".#......"
        ".####..."    // 5
        ".....#.."
        ".....#.."
        ".#...#.."
        "..###...",

        "........"
        ".....#.."
        "....#..."
        "...#...."
        "..####.."    // 6
        ".#....#."
        ".#....#."
        "..####..",

        "........"
        "........"
        ".######."
        ".....#.."
        "....#..."    // 7
        "...#...."
        "..#....."
        ".#......",

        "..####.."
        ".#....#."
        ".#....#."
        "..####.."    // 8
        ".#....#."
        ".#....#."
        ".#....#."
        "..####..",

        "..####.."
        ".#....#."
        ".#....#."
        "..#####."    // 9
        "......#."
        ".....#.."
        "....#..."
        "...#....",
    };

    for(uint32_t mip = 0; mip < 8; mip++)
    {
      uint32_t d = 128 >> mip;

      if(mip > 0)
      {
        for(uint32_t i = 0; i < d * d * (1024 >> mip); i++)
          data[i] = uint8_t((rand() % 0x7f) << 1);
      }
      else
      {
        for(uint32_t slice = 0; slice < 1024; slice++)
        {
          uint8_t *base = data + d * d * sizeof(uint8_t) * slice;

          int str[4] = {0, 0, 0, 0};

          uint32_t digitCalc = slice;

          str[0] += digitCalc / 1000;

          digitCalc %= 1000;
          str[1] += digitCalc / 100;

          digitCalc %= 100;
          str[2] += digitCalc / 10;

          digitCalc %= 10;
          str[3] += digitCalc;

          base += 32;
          base += 32 * d * sizeof(uint8_t);

          // first digit
          for(int row = 0; row < 8; row++)
            memcpy(base + row * d * sizeof(uint8_t), digits[str[0]] + row * 8, 8);

          base += 16;

          // second digit
          for(int row = 0; row < 8; row++)
            memcpy(base + row * d * sizeof(uint8_t), digits[str[1]] + row * 8, 8);

          base += 16;

          // third digit
          for(int row = 0; row < 8; row++)
            memcpy(base + row * d * sizeof(uint8_t), digits[str[2]] + row * 8, 8);

          base += 16;

          // fourth digit
          for(int row = 0; row < 8; row++)
            memcpy(base + row * d * sizeof(uint8_t), digits[str[3]] + row * 8, 8);
        }
      }

      ctx->UpdateSubresource(tex, mip, NULL, data, d * sizeof(uint8_t), d * d * sizeof(uint8_t));
    }

    ID3D11ShaderResourceViewPtr srv = MakeSRV(tex);

    delete[] data;

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->PSSetSamplers(0, 1, &samp.GetInterfacePtr());
      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
