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

const std::string dxgiFormatName[] = {
    "DXGI_FORMAT_UNKNOWN",
    "DXGI_FORMAT_R32G32B32A32_TYPELESS",
    "DXGI_FORMAT_R32G32B32A32_FLOAT",
    "DXGI_FORMAT_R32G32B32A32_UINT",
    "DXGI_FORMAT_R32G32B32A32_SINT",
    "DXGI_FORMAT_R32G32B32_TYPELESS",
    "DXGI_FORMAT_R32G32B32_FLOAT",
    "DXGI_FORMAT_R32G32B32_UINT",
    "DXGI_FORMAT_R32G32B32_SINT",
    "DXGI_FORMAT_R16G16B16A16_TYPELESS",
    "DXGI_FORMAT_R16G16B16A16_FLOAT",
    "DXGI_FORMAT_R16G16B16A16_UNORM",
    "DXGI_FORMAT_R16G16B16A16_UINT",
    "DXGI_FORMAT_R16G16B16A16_SNORM",
    "DXGI_FORMAT_R16G16B16A16_SINT",
    "DXGI_FORMAT_R32G32_TYPELESS",
    "DXGI_FORMAT_R32G32_FLOAT",
    "DXGI_FORMAT_R32G32_UINT",
    "DXGI_FORMAT_R32G32_SINT",
    "DXGI_FORMAT_R32G8X24_TYPELESS",
    "DXGI_FORMAT_D32_FLOAT_S8X24_UINT",
    "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS",
    "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT",
    "DXGI_FORMAT_R10G10B10A2_TYPELESS",
    "DXGI_FORMAT_R10G10B10A2_UNORM",
    "DXGI_FORMAT_R10G10B10A2_UINT",
    "DXGI_FORMAT_R11G11B10_FLOAT",
    "DXGI_FORMAT_R8G8B8A8_TYPELESS",
    "DXGI_FORMAT_R8G8B8A8_UNORM",
    "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",
    "DXGI_FORMAT_R8G8B8A8_UINT",
    "DXGI_FORMAT_R8G8B8A8_SNORM",
    "DXGI_FORMAT_R8G8B8A8_SINT",
    "DXGI_FORMAT_R16G16_TYPELESS",
    "DXGI_FORMAT_R16G16_FLOAT",
    "DXGI_FORMAT_R16G16_UNORM",
    "DXGI_FORMAT_R16G16_UINT",
    "DXGI_FORMAT_R16G16_SNORM",
    "DXGI_FORMAT_R16G16_SINT",
    "DXGI_FORMAT_R32_TYPELESS",
    "DXGI_FORMAT_D32_FLOAT",
    "DXGI_FORMAT_R32_FLOAT",
    "DXGI_FORMAT_R32_UINT",
    "DXGI_FORMAT_R32_SINT",
    "DXGI_FORMAT_R24G8_TYPELESS",
    "DXGI_FORMAT_D24_UNORM_S8_UINT",
    "DXGI_FORMAT_R24_UNORM_X8_TYPELESS",
    "DXGI_FORMAT_X24_TYPELESS_G8_UINT",
    "DXGI_FORMAT_R8G8_TYPELESS",
    "DXGI_FORMAT_R8G8_UNORM",
    "DXGI_FORMAT_R8G8_UINT",
    "DXGI_FORMAT_R8G8_SNORM",
    "DXGI_FORMAT_R8G8_SINT",
    "DXGI_FORMAT_R16_TYPELESS",
    "DXGI_FORMAT_R16_FLOAT",
    "DXGI_FORMAT_D16_UNORM",
    "DXGI_FORMAT_R16_UNORM",
    "DXGI_FORMAT_R16_UINT",
    "DXGI_FORMAT_R16_SNORM",
    "DXGI_FORMAT_R16_SINT",
    "DXGI_FORMAT_R8_TYPELESS",
    "DXGI_FORMAT_R8_UNORM",
    "DXGI_FORMAT_R8_UINT",
    "DXGI_FORMAT_R8_SNORM",
    "DXGI_FORMAT_R8_SINT",
    "DXGI_FORMAT_A8_UNORM",
    "DXGI_FORMAT_R1_UNORM",
    "DXGI_FORMAT_R9G9B9E5_SHAREDEXP",
    "DXGI_FORMAT_R8G8_B8G8_UNORM",
    "DXGI_FORMAT_G8R8_G8B8_UNORM",
    "DXGI_FORMAT_BC1_TYPELESS",
    "DXGI_FORMAT_BC1_UNORM",
    "DXGI_FORMAT_BC1_UNORM_SRGB",
    "DXGI_FORMAT_BC2_TYPELESS",
    "DXGI_FORMAT_BC2_UNORM",
    "DXGI_FORMAT_BC2_UNORM_SRGB",
    "DXGI_FORMAT_BC3_TYPELESS",
    "DXGI_FORMAT_BC3_UNORM",
    "DXGI_FORMAT_BC3_UNORM_SRGB",
    "DXGI_FORMAT_BC4_TYPELESS",
    "DXGI_FORMAT_BC4_UNORM",
    "DXGI_FORMAT_BC4_SNORM",
    "DXGI_FORMAT_BC5_TYPELESS",
    "DXGI_FORMAT_BC5_UNORM",
    "DXGI_FORMAT_BC5_SNORM",
    "DXGI_FORMAT_B5G6R5_UNORM",
    "DXGI_FORMAT_B5G5R5A1_UNORM",
    "DXGI_FORMAT_B8G8R8A8_UNORM",
    "DXGI_FORMAT_B8G8R8X8_UNORM",
    "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM",
    "DXGI_FORMAT_B8G8R8A8_TYPELESS",
    "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB",
    "DXGI_FORMAT_B8G8R8X8_TYPELESS",
    "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB",
    "DXGI_FORMAT_BC6H_TYPELESS",
    "DXGI_FORMAT_BC6H_UF16",
    "DXGI_FORMAT_BC6H_SF16",
    "DXGI_FORMAT_BC7_TYPELESS",
    "DXGI_FORMAT_BC7_UNORM",
    "DXGI_FORMAT_BC7_UNORM_SRGB",
    "DXGI_FORMAT_AYUV",
    "DXGI_FORMAT_Y410",
    "DXGI_FORMAT_Y416",
    "DXGI_FORMAT_NV12",
    "DXGI_FORMAT_P010",
    "DXGI_FORMAT_P016",
    "DXGI_FORMAT_420_OPAQUE",
    "DXGI_FORMAT_YUY2",
    "DXGI_FORMAT_Y210",
    "DXGI_FORMAT_Y216",
    "DXGI_FORMAT_NV11",
    "DXGI_FORMAT_AI44",
    "DXGI_FORMAT_IA44",
    "DXGI_FORMAT_P8",
    "DXGI_FORMAT_A8P8",
    "DXGI_FORMAT_B4G4R4A4_UNORM",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "DXGI_FORMAT_P208",
    "DXGI_FORMAT_V208",
    "DXGI_FORMAT_V408",
};

RD_TEST(D3D11_Pixel_History_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Checks pixel history on different formats, scenarios, overdraw, etc.";

  bool IsUIntFormat(DXGI_FORMAT f)
  {
    switch(f)
    {
      case DXGI_FORMAT_R32G32B32A32_UINT:
      case DXGI_FORMAT_R32G32B32_UINT:
      case DXGI_FORMAT_R16G16B16A16_UINT:
      case DXGI_FORMAT_R32G32_UINT:
      case DXGI_FORMAT_R10G10B10A2_UINT:
      case DXGI_FORMAT_R8G8B8A8_UINT:
      case DXGI_FORMAT_R16G16_UINT:
      case DXGI_FORMAT_R32_UINT:
      case DXGI_FORMAT_R8G8_UINT:
      case DXGI_FORMAT_R16_UINT:
      case DXGI_FORMAT_R8_UINT: return true;
      default: break;
    }

    return false;
  }

  bool IsSIntFormat(DXGI_FORMAT f)
  {
    switch(f)
    {
      case DXGI_FORMAT_R32G32B32A32_SINT:
      case DXGI_FORMAT_R32G32B32_SINT:
      case DXGI_FORMAT_R16G16B16A16_SINT:
      case DXGI_FORMAT_R32G32_SINT:
      case DXGI_FORMAT_R8G8B8A8_SINT:
      case DXGI_FORMAT_R16G16_SINT:
      case DXGI_FORMAT_R32_SINT:
      case DXGI_FORMAT_R8G8_SINT:
      case DXGI_FORMAT_R16_SINT:
      case DXGI_FORMAT_R8_SINT: return true;
      default: break;
    }

    return false;
  }

  DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT f)
  {
    switch(f)
    {
      case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_UINT:
      case DXGI_FORMAT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_TYPELESS;

      case DXGI_FORMAT_R32G32B32_TYPELESS:
      case DXGI_FORMAT_R32G32B32_FLOAT:
      case DXGI_FORMAT_R32G32B32_UINT:
      case DXGI_FORMAT_R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_TYPELESS;

      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R16G16B16A16_UNORM:
      case DXGI_FORMAT_R16G16B16A16_UINT:
      case DXGI_FORMAT_R16G16B16A16_SNORM:
      case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_TYPELESS;

      case DXGI_FORMAT_R32G32_TYPELESS:
      case DXGI_FORMAT_R32G32_FLOAT:
      case DXGI_FORMAT_R32G32_UINT:
      case DXGI_FORMAT_R32G32_SINT: return DXGI_FORMAT_R32G32_TYPELESS;

      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;

      case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
      case DXGI_FORMAT_R10G10B10A2_UINT:
      case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:    // maybe not valid cast?
        return DXGI_FORMAT_R10G10B10A2_TYPELESS;

      case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_R8G8B8A8_UINT:
      case DXGI_FORMAT_R8G8B8A8_SNORM:
      case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_TYPELESS;

      case DXGI_FORMAT_R16G16_TYPELESS:
      case DXGI_FORMAT_R16G16_FLOAT:
      case DXGI_FORMAT_R16G16_UNORM:
      case DXGI_FORMAT_R16G16_UINT:
      case DXGI_FORMAT_R16G16_SNORM:
      case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_TYPELESS;

      case DXGI_FORMAT_R32_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT:    // maybe not valid cast?
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_UINT:
      case DXGI_FORMAT_R32_SINT: return DXGI_FORMAT_R32_TYPELESS;

      // maybe not valid casts?
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;

      case DXGI_FORMAT_B8G8R8A8_TYPELESS:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      case DXGI_FORMAT_R8G8_B8G8_UNORM:    // maybe not valid cast?
      case DXGI_FORMAT_G8R8_G8B8_UNORM:    // maybe not valid cast?
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;

      case DXGI_FORMAT_B8G8R8X8_UNORM:
      case DXGI_FORMAT_B8G8R8X8_TYPELESS:
      case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_TYPELESS;

      case DXGI_FORMAT_R8G8_TYPELESS:
      case DXGI_FORMAT_R8G8_UNORM:
      case DXGI_FORMAT_R8G8_UINT:
      case DXGI_FORMAT_R8G8_SNORM:
      case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_TYPELESS;

      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_R16_FLOAT:
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      case DXGI_FORMAT_R16_SNORM:
      case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_TYPELESS;

      case DXGI_FORMAT_R8_TYPELESS:
      case DXGI_FORMAT_R8_UNORM:
      case DXGI_FORMAT_R8_UINT:
      case DXGI_FORMAT_R8_SNORM:
      case DXGI_FORMAT_R8_SINT: return DXGI_FORMAT_R8_TYPELESS;

      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
      case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_TYPELESS;

      case DXGI_FORMAT_BC4_TYPELESS:
      case DXGI_FORMAT_BC4_UNORM:
      case DXGI_FORMAT_BC4_SNORM: return DXGI_FORMAT_BC4_TYPELESS;

      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
      case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_TYPELESS;

      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
      case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_TYPELESS;

      case DXGI_FORMAT_BC5_TYPELESS:
      case DXGI_FORMAT_BC5_UNORM:
      case DXGI_FORMAT_BC5_SNORM: return DXGI_FORMAT_BC5_TYPELESS;

      case DXGI_FORMAT_BC6H_TYPELESS:
      case DXGI_FORMAT_BC6H_UF16:
      case DXGI_FORMAT_BC6H_SF16: return DXGI_FORMAT_BC6H_TYPELESS;

      case DXGI_FORMAT_BC7_TYPELESS:
      case DXGI_FORMAT_BC7_UNORM:
      case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_TYPELESS;

      case DXGI_FORMAT_R1_UNORM:
      case DXGI_FORMAT_A8_UNORM:
      case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
      case DXGI_FORMAT_B5G6R5_UNORM:
      case DXGI_FORMAT_B5G5R5A1_UNORM:
      case DXGI_FORMAT_R11G11B10_FLOAT:
      case DXGI_FORMAT_AYUV:
      case DXGI_FORMAT_Y410:
      case DXGI_FORMAT_YUY2:
      case DXGI_FORMAT_Y416:
      case DXGI_FORMAT_NV12:
      case DXGI_FORMAT_P010:
      case DXGI_FORMAT_P016:
      case DXGI_FORMAT_420_OPAQUE:
      case DXGI_FORMAT_Y210:
      case DXGI_FORMAT_Y216:
      case DXGI_FORMAT_NV11:
      case DXGI_FORMAT_AI44:
      case DXGI_FORMAT_IA44:
      case DXGI_FORMAT_P8:
      case DXGI_FORMAT_A8P8:
      case DXGI_FORMAT_P208:
      case DXGI_FORMAT_V208:
      case DXGI_FORMAT_V408:
      case DXGI_FORMAT_B4G4R4A4_UNORM: return f;

      case DXGI_FORMAT_UNKNOWN: return DXGI_FORMAT_UNKNOWN;

      default: return DXGI_FORMAT_UNKNOWN;
    }
  }

  std::string vertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f main(vertin IN, uint vid : SV_VertexID)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.xy, 0.5f, 1.0f);
	OUT.col = IN.col;
	OUT.uv = IN.uv;

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

cbuffer refcounter : register(b0)
{
  uint expected;
};

cbuffer uavcounter : register(b1)
{
  uint actual;
};

float4 main() : SV_Target0
{
  if(expected != actual)
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
	return float4(0.0f, 1.0f, 0.1234f, 0.5f);
}

)EOSHADER";

  std::string pixelUInt = R"EOSHADER(

cbuffer refcounter : register(b0)
{
  uint expected;
};

cbuffer uavcounter : register(b1)
{
  uint actual;
};

uint4 main() : SV_Target0
{
  if(expected != actual)
    return uint4(1, 0, 0, 1);
	return uint4(0, 1, 1234, 5);
}

)EOSHADER";

  std::string pixelSInt = R"EOSHADER(

cbuffer refcounter : register(b0)
{
  uint expected;
};

cbuffer uavcounter : register(b1)
{
  uint actual;
};

int4 main() : SV_Target0
{
  if(expected != actual)
    return int4(1, 0, 0, 1);
	return int4(0, 1, -1234, 5);
}

)EOSHADER";

  std::string compute = R"EOSHADER(

RWBuffer<uint> buf : register(u0);

[numthreads(1,1,1)]
void main()
{
	InterlockedAdd(buf[0], 1);
}

)EOSHADER";

  std::string pixelUAVWrite = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

RWTexture2D<float4> uavOut;

float4 main(v2f IN) : SV_Target0
{
  uavOut[IN.pos.xy*0.5] = float4(IN.uv.x, IN.uv.y, 0.0f, 1.0f);
	return float4(0.1234, 1.0f, 0.0f, 0.5f);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(Compile(pixel, "main", "ps_4_0"));
    ID3D11PixelShaderPtr psUInt = CreatePS(Compile(pixelUInt, "main", "ps_4_0"));
    ID3D11PixelShaderPtr psSInt = CreatePS(Compile(pixelSInt, "main", "ps_4_0"));
    ID3D11PixelShaderPtr psUAVWrite = CreatePS(Compile(pixelUAVWrite, "main", "ps_5_0"));

    ID3D11ComputeShaderPtr cs = CreateCS(Compile(compute, "main", "cs_5_0"));

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    const DXGI_FORMAT depthFormats[] = {
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D16_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_R32G8X24_TYPELESS,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    };

    const DXGI_FORMAT dsvFormats[] = {
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D16_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    };

    static_assert(sizeof(depthFormats) == sizeof(dsvFormats),
                  "depth arrays should be identical sizes.");

    ID3D11BufferPtr bufRef = MakeBuffer().Size(256).Constant();
    ID3D11BufferPtr bufCounter = MakeBuffer().Size(256).UAV();
    ID3D11BufferPtr bufCounterCB = MakeBuffer().Size(256).Constant();
    ID3D11UnorderedAccessViewPtr bufCounterUAV = MakeUAV(bufCounter).Format(DXGI_FORMAT_R32_UINT);

    ctx->CSSetUnorderedAccessViews(0, 1, &bufCounterUAV.GetInterfacePtr(), NULL);
    ctx->CSSetShader(cs, NULL, 0);

    std::vector<ID3D11DepthStencilViewPtr> dsvs;
    std::vector<ID3D11RenderTargetViewPtr> rts;

    for(size_t i = 0; i < ARRAY_COUNT(dsvFormats); i++)
    {
      // normal
      dsvs.push_back(
          MakeDSV(MakeTexture(depthFormats[i], 16, 16).DSV().Tex2D()).Format(dsvFormats[i]));

      // submip and subslice selected
      dsvs.push_back(MakeDSV(MakeTexture(depthFormats[i], 32, 32).Array(32).DSV().Mips(2).Tex2D())
                         .Format(dsvFormats[i])
                         .FirstMip(1)
                         .NumMips(1)
                         .FirstSlice(4)
                         .NumSlices(1));
    }

    const DXGI_FORMAT colorFormats[] = {
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_UINT,
        DXGI_FORMAT_R32G32B32A32_SINT,
        DXGI_FORMAT_R32G32B32_FLOAT,
        DXGI_FORMAT_R32G32B32_UINT,
        DXGI_FORMAT_R32G32B32_SINT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R16G16B16A16_UNORM,
        DXGI_FORMAT_R16G16B16A16_UINT,
        DXGI_FORMAT_R16G16B16A16_SNORM,
        DXGI_FORMAT_R16G16B16A16_SINT,
        DXGI_FORMAT_R32G32_FLOAT,
        DXGI_FORMAT_R32G32_UINT,
        DXGI_FORMAT_R32G32_SINT,
        DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R10G10B10A2_UINT,
        DXGI_FORMAT_R11G11B10_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UINT,
        DXGI_FORMAT_R8G8B8A8_SNORM,
        DXGI_FORMAT_R8G8B8A8_SINT,
        DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R16G16_UNORM,
        DXGI_FORMAT_R16G16_UINT,
        DXGI_FORMAT_R16G16_SNORM,
        DXGI_FORMAT_R16G16_SINT,
        DXGI_FORMAT_R32_FLOAT,
        DXGI_FORMAT_R32_UINT,
        DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R8G8_UNORM,
        DXGI_FORMAT_R8G8_UINT,
        DXGI_FORMAT_R8G8_SNORM,
        DXGI_FORMAT_R8G8_SINT,
        DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R16_UNORM,
        DXGI_FORMAT_R16_UINT,
        DXGI_FORMAT_R16_SNORM,
        DXGI_FORMAT_R16_SINT,
        DXGI_FORMAT_R8_UNORM,
        DXGI_FORMAT_R8_UINT,
        DXGI_FORMAT_R8_SNORM,
        DXGI_FORMAT_R8_SINT,
        DXGI_FORMAT_A8_UNORM,
        DXGI_FORMAT_R1_UNORM,
        DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
        DXGI_FORMAT_B5G6R5_UNORM,
        DXGI_FORMAT_B5G5R5A1_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_B8G8R8X8_UNORM,
        DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
        DXGI_FORMAT_B4G4R4A4_UNORM,
    };

    for(size_t i = 0; i < ARRAY_COUNT(colorFormats); i++)
    {
      DXGI_FORMAT f = colorFormats[i];

      UINT supp = 0;
      dev->CheckFormatSupport(f, &supp);

      if((supp & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0)
        continue;

      std::vector<DXGI_FORMAT> fmts = {f};

      // test typeless->casted for the first three (RGBA32)
      if(i < 3)
        fmts.push_back(GetTypelessFormat(f));

      for(DXGI_FORMAT tex_fmt : fmts)
      {
        // make a normal one
        rts.push_back(MakeRTV(MakeTexture(tex_fmt, 16, 16).RTV().Tex2D()).Format(f));

        // make a subslice and submip one
        rts.push_back(MakeRTV(MakeTexture(tex_fmt, 32, 32).Array(32).Mips(2).RTV().Tex2D())
                          .Format(f)
                          .FirstMip(1)
                          .NumMips(1)
                          .FirstSlice(4)
                          .NumSlices(1));
      }
    }

    // make a simple dummy texture for MRT testing
    ID3D11RenderTargetViewPtr mrt =
        MakeRTV(MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 16, 16).RTV().Tex2D());

    // texture for UAV write testing
    ID3D11UnorderedAccessViewPtr uavView =
        MakeUAV(MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 8, 8).UAV().Tex2D());

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, 16.0f, 16.0f, 0.0f, 1.0f});

      UINT zero[4] = {};
      ctx->ClearUnorderedAccessViewUint(bufCounterUAV, zero);

      uint32_t testCounter = 0;

      // for each DSV and for none
      for(size_t dsvIdx = 0; dsvIdx <= dsvs.size(); dsvIdx++)
      {
        ID3D11DepthStencilViewPtr dsv = dsvIdx < dsvs.size() ? dsvs[dsvIdx] : NULL;

        DXGI_FORMAT df = DXGI_FORMAT_UNKNOWN;
        if(dsv)
        {
          D3D11_DEPTH_STENCIL_VIEW_DESC desc;
          dsv->GetDesc(&desc);
          df = desc.Format;
        }

        // for each RT
        for(size_t rtIdx = 0; rtIdx < rts.size(); rtIdx++)
        {
          ID3D11RenderTargetViewPtr rt = rts[rtIdx];

          ID3D11RenderTargetViewPtr dummy = mrt;
          DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN;
          {
            D3D11_RENDER_TARGET_VIEW_DESC desc;
            rt->GetDesc(&desc);
            f = desc.Format;
          }

          // for all but the first DSV and the last (none) DSV, skip the bulk of the colour tests to
          // reduce the test matrix.
          if(dsvIdx > 0 && dsvIdx < dsvs.size())
          {
            if(rtIdx > 10)
              break;
          }

          pushMarker("Test RTV: " + dxgiFormatName[f] +
                     " & depth: " + (df == DXGI_FORMAT_UNKNOWN ? "None" : dxgiFormatName[df]));

          ctx->OMSetRenderTargets(1, &rt.GetInterfacePtr(), dsv);

          // dispatch the CS to increment the buffer counter on the GPU
          ctx->Dispatch(1, 1, 1);
          // increment the CPU counter
          testCounter++;

          // update CBs so we can check for validity
          uint32_t data[256 / 4];
          data[0] = testCounter;
          ctx->UpdateSubresource(bufRef, 0, NULL, data, 256, 256);
          ctx->CopyResource(bufCounterCB, bufCounter);

          ctx->PSSetConstantBuffers(0, 1, &bufRef.GetInterfacePtr());
          ctx->PSSetConstantBuffers(1, 1, &bufCounterCB.GetInterfacePtr());

          setMarker("Test " + std::to_string(testCounter));

          if(IsUIntFormat(f))
          {
            setMarker("UInt tex");
            ctx->PSSetShader(psUInt, NULL, 0);
          }
          else if(IsSIntFormat(f))
          {
            setMarker("SInt tex");
            ctx->PSSetShader(psSInt, NULL, 0);
          }
          else
          {
            setMarker("Float tex");
            ctx->PSSetShader(ps, NULL, 0);
          }

          if(dsv)
          {
            setMarker("DSVClear");
            ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
          }

          setMarker("RTVClear");
          ClearRenderTargetView(rt, Vec4f(1.0f, 0.0f, 1.0f, 1.0f));

          setMarker("BasicDraw");
          ctx->Draw(3, 0);

          popMarker();
        }
      }

      {
        float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
        ctx->ClearUnorderedAccessViewFloat(uavView.GetInterfacePtr(), white);
        ctx->PSSetShader(psUAVWrite, NULL, 0);
        ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rts[0].GetInterfacePtr(), NULL, 1, 1,
                                                       &uavView.GetInterfacePtr(), NULL);

        setMarker("UAVWrite");
        ctx->Draw(3, 0);
        ctx->PSSetShader(psUAVWrite, NULL, 0);
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
