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

using namespace TextureZoo;

RD_TEST(D3D11_Texture_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Tests all possible combinations of texture type and format that are supported.";

  std::string pixelTemplate = R"EOSHADER( intex : register(t0);

float4 main() : SV_Target0
{
	return intex.Load(&params).&swizzle;
}
)EOSHADER";

  std::string pixelBlit = R"EOSHADER(

Texture2D<float4> intex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target0
{
	return intex.Load(float3(pos.xy, 0));
}

)EOSHADER";

  std::string pixelMSFloat = R"EOSHADER(

cbuffer consts : register(b0)
{
	uint slice;
	uint mip;
  uint flags;
  uint z;
};

float srgb2linear(float f)
{
  if (f <= 0.04045f)
    return f / 12.92f;
  else
    return pow((f + 0.055f) / 1.055f, 2.4f);
}

float4 main(float4 pos : SV_Position, uint samp : SV_SampleIndex) : SV_Target0
{
  uint x = uint(pos.x);
  uint y = uint(pos.y);

  float4 ret = float4(0.1f, 0.35f, 0.6f, 0.85f);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + z) % max(1u, TEX_WIDTH >> mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += 0.075f.xxxx * (samp + mip);

  // Signed normals are negative
  if(flags & 1)
    ret = -ret;

  // undo SRGB curve applied in output merger, to match the textures we just blat values into
  // without conversion (which are then interpreted as srgb implicitly)
  if(flags & 2)
  {
    ret.r = srgb2linear(ret.r);
    ret.g = srgb2linear(ret.g);
    ret.b = srgb2linear(ret.b);
  }

  // BGR flip - same as above, for BGRA textures
  if(flags & 4)
    ret.rgb = ret.bgr;

   // put red into alpha, because that's what we did in manual upload
  if(flags & 8)
    ret.a = ret.r;

  return ret;
}

)EOSHADER";

  std::string pixelMSDepth = R"EOSHADER(

cbuffer consts : register(b0)
{
	uint slice;
	uint mip;
  uint flags;
  uint z;
};

float main(float4 pos : SV_Position, uint samp : SV_SampleIndex) : SV_Depth
{
  uint x = uint(pos.x);
  uint y = uint(pos.y);

  float ret = 0.1f;

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + z) % max(1u, TEX_WIDTH >> mip);

  // pixels off the diagonal invert the colors
  // second slice adds a coarse checkerboard pattern of inversion
  if((offs_x != y) != (slice > 0 && (((x / 2) % 2) != ((y / 2) % 2))))
  {
    ret = 0.85f;

    // so we can fill stencil data, clip off the inverted values
    if(flags == 1)
      clip(-1);
  }

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += 0.075f * (samp + mip);

  return ret;
}

)EOSHADER";

  std::string pixelMSUInt = R"EOSHADER(

cbuffer consts : register(b0)
{
	uint slice;
	uint mip;
  uint flags;
  uint z;
};

uint4 main(float4 pos : SV_Position, uint samp : SV_SampleIndex) : SV_Target0
{
  uint x = uint(pos.x);
  uint y = uint(pos.y);

  uint4 ret = uint4(10, 40, 70, 100);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + z) % max(1u, TEX_WIDTH >> mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += uint4(10, 10, 10, 10) * (samp + mip);

  return ret;
}

)EOSHADER";

  std::string pixelMSSInt = R"EOSHADER(

cbuffer consts : register(b0)
{
	uint slice;
	uint mip;
  uint flags;
  uint z;
};

int4 main(float4 pos : SV_Position, uint samp : SV_SampleIndex) : SV_Target0
{
  uint x = uint(pos.x);
  uint y = uint(pos.y);

  int4 ret = int4(10, 40, 70, 100);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + z) % max(1u, TEX_WIDTH >> mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += int4(10, 10, 10, 10) * (samp + mip);

  return -ret;
}

)EOSHADER";

  struct D3D11Format
  {
    const std::string name;
    DXGI_FORMAT texFmt;
    DXGI_FORMAT viewFmt;
    TexConfig cfg;
  };

  struct TestCase
  {
    D3D11Format fmt;
    uint32_t dim;
    bool isArray;
    bool canRender;
    bool isDepth;
    bool isMSAA;
    bool hasData;
    ID3D11ResourcePtr res;
    ID3D11ShaderResourceViewPtr srv;
  };

  std::string MakeName(const TestCase &test)
  {
    std::string name = "Texture " + std::to_string(test.dim) + "D";

    if(test.isMSAA)
      name += " MSAA";
    if(test.isArray)
      name += " Array";

    return name;
  }

  ID3D11PixelShaderPtr GetShader(const TestCase &test)
  {
    static std::map<uint32_t, ID3D11PixelShaderPtr> shaders;

    bool isStencilOut = (test.fmt.viewFmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
                         test.fmt.viewFmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT);

    uint32_t key = uint32_t(test.fmt.cfg.data);
    key |= test.dim << 6;
    key |= test.isMSAA ? 0x80000 : 0;
    key |= isStencilOut ? 0x100000 : 0;

    ID3D11PixelShaderPtr ret = shaders[key];
    if(!ret)
    {
      std::string texType = "Texture" + std::to_string(test.dim) + "D";
      if(test.isMSAA)
        texType += "MS";
      if(test.dim < 3)
        texType += "Array";

      const std::string innerType[] = {
          "float", "unorm float", "float", "uint", "int",
      };
      static_assert(ARRAY_COUNT(innerType) == (size_t)DataType::Count,
                    "DataType has changed, update array");

      texType += "<" + innerType[(int)test.fmt.cfg.data] + "4>";

      std::string src = texType + pixelTemplate;

      if(test.isMSAA)
        src.replace(src.find("&params"), 7, "0, 0");
      else
        src.replace(src.find("&params"), 7, "0");

      if(isStencilOut)
        src.replace(src.find("&swizzle"), 8, "zyzz*float4(0,1,0,0)");
      else
        src.replace(src.find("&swizzle"), 8, "xyzw");

      ret = shaders[key] = CreatePS(Compile(src, "main", "ps_5_0"));
    }

    return ret;
  }

  bool SetData(ID3D11ResourcePtr res, const D3D11Format &fmt)
  {
    Vec4i dim;
    UINT mips, slices;
    GetDimensions(res, dim, mips, slices);

    TexData data;

    for(UINT s = 0; s < slices; s++)
    {
      for(UINT m = 0; m < mips; m++)
      {
        MakeData(data, fmt.cfg, dim, m, s);

        if(data.byteData.empty())
          return false;

        ctx->UpdateSubresource(res, s * mips + m, NULL, data.byteData.data(), data.rowPitch,
                               data.slicePitch);
      }
    }

    return true;
  }

  void GetDimensions(ID3D11ResourcePtr res, Vec4i & dim, UINT & mips, UINT & slices)
  {
    dim = Vec4i();
    mips = 1;
    slices = 1;

    D3D11_RESOURCE_DIMENSION t;
    res->GetType(&t);

    if(t == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
    {
      ID3D11Texture1DPtr tex = res;
      D3D11_TEXTURE1D_DESC desc;
      tex->GetDesc(&desc);

      dim.x = desc.Width;
      mips = std::max(mips, desc.MipLevels);
      slices = std::max(slices, desc.ArraySize);
    }
    else if(t == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
    {
      ID3D11Texture2DPtr tex = res;
      D3D11_TEXTURE2D_DESC desc;
      tex->GetDesc(&desc);

      dim.x = desc.Width;
      dim.y = desc.Height;
      mips = std::max(mips, desc.MipLevels);
      slices = std::max(slices, desc.ArraySize);
    }
    else if(t == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
    {
      ID3D11Texture3DPtr tex = res;
      D3D11_TEXTURE3D_DESC desc;
      tex->GetDesc(&desc);

      dim.x = desc.Width;
      dim.y = desc.Height;
      dim.z = desc.Depth;
      mips = std::max(mips, desc.MipLevels);
    }
    else
    {
      TEST_ERROR("Unexpected resource type!");
    }
  }

  TestCase FinaliseTest(TestCase test)
  {
    if(test.dim == 1)
    {
      D3D11TextureCreator creator =
          MakeTexture(test.fmt.texFmt, texWidth).Mips(texMips).Array(test.isArray ? texSlices : 1).SRV();

      if(test.isDepth)
        creator.DSV();
      else if(test.canRender)
        creator.RTV();

      ID3D11Texture1DPtr tex = creator;

      test.srv = MakeSRV(tex).Format(test.fmt.viewFmt);
      test.res = tex;
    }
    else if(test.dim == 2 && !test.isMSAA)
    {
      D3D11TextureCreator creator = MakeTexture(test.fmt.texFmt, texWidth, texHeight)
                                        .Mips(texMips)
                                        .Array(test.isArray ? texSlices : 1)
                                        .SRV();

      if(test.isDepth)
        creator.DSV();
      else if(test.canRender)
        creator.RTV();

      ID3D11Texture2DPtr tex = creator;

      test.srv = MakeSRV(tex).Format(test.fmt.viewFmt);
      test.res = tex;
    }
    else if(test.dim == 2 && test.isMSAA)
    {
      D3D11TextureCreator creator = MakeTexture(test.fmt.texFmt, texWidth, texHeight)
                                        .Multisampled(texSamples)
                                        .Array(test.isArray ? texSlices : 1)
                                        .SRV();

      if(test.isDepth)
        creator.DSV();
      else
        creator.RTV();

      ID3D11Texture2DPtr tex = creator;

      test.srv = MakeSRV(tex).Format(test.fmt.viewFmt);
      test.res = tex;
      test.canRender = true;
    }
    else if(test.dim == 3)
    {
      D3D11TextureCreator creator =
          MakeTexture(test.fmt.texFmt, texWidth, texHeight, texDepth).Mips(texMips).SRV();

      if(test.canRender)
        creator.RTV();

      ID3D11Texture3DPtr tex = creator;

      test.srv = MakeSRV(tex).Format(test.fmt.viewFmt);
      test.res = tex;
    }

    SetDebugName(test.res, MakeName(test) + " " + test.fmt.name);

    // discard the resource when possible, this makes renderdoc treat it as dirty
    if(ctx1)
      ctx1->DiscardResource(test.res);

    if(!test.isMSAA)
    {
      pushMarker("Set data for " + test.fmt.name + " " + MakeName(test));

      test.hasData = SetData(test.res, test.fmt);

      popMarker();
    }

    return test;
  }

  DXGI_FORMAT GetDepthFormat(const D3D11Format &f)
  {
    DXGI_FORMAT queryFormat = DXGI_FORMAT_UNKNOWN;

    switch(f.texFmt)
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS: queryFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; break;
      case DXGI_FORMAT_R24G8_TYPELESS: queryFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
      case DXGI_FORMAT_R32_TYPELESS: queryFormat = DXGI_FORMAT_D32_FLOAT; break;
      case DXGI_FORMAT_R16_TYPELESS: queryFormat = DXGI_FORMAT_D16_UNORM; break;
      default: TEST_ERROR("Unexpected base texture format");
    }

    return queryFormat;
  }

  void AddSupportedTests(const D3D11Format &f, std::vector<TestCase> &test_textures, bool depthMode)
  {
    DXGI_FORMAT queryFormat = f.viewFmt;

    if(depthMode)
      queryFormat = GetDepthFormat(f);

    UINT supp = 0;
    dev->CheckFormatSupport(queryFormat, &supp);

    bool renderable = (supp & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
    bool depth = (supp & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) != 0;

    if((supp & D3D11_FORMAT_SUPPORT_SHADER_LOAD) || depth)
    {
      // TODO: disable 1D depth textures for now, we don't support displaying them
      if(!depthMode)
      {
        if(supp & D3D11_FORMAT_SUPPORT_TEXTURE1D)
        {
          test_textures.push_back(FinaliseTest({f, 1, false, renderable, depth}));
          test_textures.push_back(FinaliseTest({f, 1, true, renderable, depth}));
        }
        else
        {
          test_textures.push_back({f, 1, false});
          test_textures.push_back({f, 1, true});
        }
      }

      if(supp & D3D11_FORMAT_SUPPORT_TEXTURE2D)
      {
        test_textures.push_back(FinaliseTest({f, 2, false, renderable, depth}));
        test_textures.push_back(FinaliseTest({f, 2, true, renderable, depth}));
      }
      else
      {
        test_textures.push_back({f, 2, false});
        test_textures.push_back({f, 2, true});
      }
      if(supp & D3D11_FORMAT_SUPPORT_TEXTURE3D)
      {
        test_textures.push_back(FinaliseTest({f, 3, false, renderable, depth}));
      }
      else
      {
        test_textures.push_back({f, 3, false});
      }
      if(((supp & D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD) || depth) &&
         (supp & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET))
      {
        test_textures.push_back(FinaliseTest({f, 2, false, true, depth, true}));
        test_textures.push_back(FinaliseTest({f, 2, true, true, depth, true}));
      }
      else
      {
        test_textures.push_back({f, 2, false, true, depth, true});
        test_textures.push_back({f, 2, true, true, depth, true});
      }
    }
    else
    {
      test_textures.push_back({f, 2, false});

      if(supp & (D3D11_FORMAT_SUPPORT_TEXTURE1D | D3D11_FORMAT_SUPPORT_TEXTURE2D |
                 D3D11_FORMAT_SUPPORT_TEXTURE3D))
      {
        TEST_ERROR("Format %d can't be loaded in shader but can be a texture!", f.texFmt);
      }
    }
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11VertexShaderPtr vs = CreateVS(Compile(D3DFullscreenQuadVertex, "main", "vs_4_0"));
    ID3D11PixelShaderPtr blitps = CreatePS(Compile(pixelBlit, "main", "ps_4_0"));

    pushMarker("Add tests");

#define TEST_CASE_NAME(texFmt, viewFmt)           \
  (texFmt == viewFmt) ? std::string(#texFmt + 12) \
                      : (std::string(#texFmt + 12) + "->" + (strchr(#viewFmt + 12, '_') + 1))

#define TEST_CASE(texType, texFmt, viewFmt, compCount, byteWidth, dataType) \
  {                                                                         \
      TEST_CASE_NAME(texFmt, viewFmt),                                      \
      texFmt,                                                               \
      viewFmt,                                                              \
      {texType, compCount, byteWidth, dataType},                            \
  }

    std::vector<TestCase> test_textures;

    const D3D11Format color_tests[] = {
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32A32_TYPELESS,
                  DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32A32_TYPELESS,
                  DXGI_FORMAT_R32G32B32A32_UINT, 4, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32A32_FLOAT,
                  DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32A32_UINT,
                  DXGI_FORMAT_R32G32B32A32_UINT, 4, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32A32_SINT,
                  DXGI_FORMAT_R32G32B32A32_SINT, 4, 4, DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32_TYPELESS, DXGI_FORMAT_R32G32B32_FLOAT,
                  3, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32_TYPELESS, DXGI_FORMAT_R32G32B32_UINT,
                  3, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, 3,
                  4, DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_UINT, 3,
                  4, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32_SINT, 3,
                  4, DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_TYPELESS,
                  DXGI_FORMAT_R16G16B16A16_FLOAT, 4, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_TYPELESS,
                  DXGI_FORMAT_R16G16B16A16_UINT, 4, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_TYPELESS,
                  DXGI_FORMAT_R16G16B16A16_UNORM, 4, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_FLOAT,
                  DXGI_FORMAT_R16G16B16A16_FLOAT, 4, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_UNORM,
                  DXGI_FORMAT_R16G16B16A16_UNORM, 4, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_UINT,
                  DXGI_FORMAT_R16G16B16A16_UINT, 4, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_SNORM,
                  DXGI_FORMAT_R16G16B16A16_SNORM, 4, 2, DataType::SNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16B16A16_SINT,
                  DXGI_FORMAT_R16G16B16A16_SINT, 4, 2, DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32_TYPELESS, DXGI_FORMAT_R32G32_FLOAT, 2, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32_TYPELESS, DXGI_FORMAT_R32G32_UINT, 2, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, 2, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_UINT, 2, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32_SINT, 2, 4,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT, 4,
                  1, DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM,
                  4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, 4,
                  1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_UINT, 4, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SNORM, 4,
                  1, DataType::SNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_R8G8B8A8_SINT, 4, 1,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_FLOAT, 2, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_UINT, 2, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_UNORM, 2, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, 2, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UNORM, 2, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16_UINT, 2, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_SNORM, 2, 2,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_R16G16_SINT, 2, 2,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, 1, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_UINT, 1, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, 1, 4,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, 1, 4,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32_SINT, 1, 4,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_UINT, 2, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_UNORM, 2, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM, 2, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8_UINT, 2, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_SNORM, 2, 1,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8G8_SINT, DXGI_FORMAT_R8G8_SINT, 2, 1,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_FLOAT, 1, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UINT, 1, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, 1, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT, 1, 2,
                  DataType::Float),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM, 1, 2,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT, 1, 2,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R16_SNORM, 1, 2,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R16_SINT, 1, 2,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UINT, 1, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UNORM, 1, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM, 1, 1,
                  DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_UINT, 1, 1,
                  DataType::UInt),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R8_SNORM, 1, 1,
                  DataType::SNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_R8_SINT, DXGI_FORMAT_R8_SINT, 1, 1,
                  DataType::SInt),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM,
                  4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8A8_TYPELESS,
                  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, 4,
                  1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 4, 1, DataType::UNorm),

        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8X8_TYPELESS, DXGI_FORMAT_B8G8R8X8_UNORM,
                  4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8X8_TYPELESS,
                  DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM, 4,
                  1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
                  DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM, 1, 1,
                  DataType::UNorm),

        TEST_CASE(TextureType::BC1, DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC1, DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM_SRGB, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC1, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC1, DXGI_FORMAT_BC1_UNORM_SRGB, DXGI_FORMAT_BC1_UNORM_SRGB, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::BC2, DXGI_FORMAT_BC2_TYPELESS, DXGI_FORMAT_BC2_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC2, DXGI_FORMAT_BC2_TYPELESS, DXGI_FORMAT_BC2_UNORM_SRGB, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC2, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC2, DXGI_FORMAT_BC2_UNORM_SRGB, DXGI_FORMAT_BC2_UNORM_SRGB, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::BC3, DXGI_FORMAT_BC3_TYPELESS, DXGI_FORMAT_BC3_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC3, DXGI_FORMAT_BC3_TYPELESS, DXGI_FORMAT_BC3_UNORM_SRGB, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC3, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC3, DXGI_FORMAT_BC3_UNORM_SRGB, DXGI_FORMAT_BC3_UNORM_SRGB, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::BC4, DXGI_FORMAT_BC4_TYPELESS, DXGI_FORMAT_BC4_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC4, DXGI_FORMAT_BC4_TYPELESS, DXGI_FORMAT_BC4_SNORM, 0, 0,
                  DataType::SNorm),
        TEST_CASE(TextureType::BC4, DXGI_FORMAT_BC4_UNORM, DXGI_FORMAT_BC4_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC4, DXGI_FORMAT_BC4_SNORM, DXGI_FORMAT_BC4_SNORM, 0, 0,
                  DataType::SNorm),

        TEST_CASE(TextureType::BC5, DXGI_FORMAT_BC5_TYPELESS, DXGI_FORMAT_BC5_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC5, DXGI_FORMAT_BC5_TYPELESS, DXGI_FORMAT_BC5_SNORM, 0, 0,
                  DataType::SNorm),
        TEST_CASE(TextureType::BC5, DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC5_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC5, DXGI_FORMAT_BC5_SNORM, DXGI_FORMAT_BC5_SNORM, 0, 0,
                  DataType::SNorm),

        TEST_CASE(TextureType::BC6, DXGI_FORMAT_BC6H_TYPELESS, DXGI_FORMAT_BC6H_UF16, 0, 0,
                  DataType::Float),
        TEST_CASE(TextureType::BC6, DXGI_FORMAT_BC6H_TYPELESS, DXGI_FORMAT_BC6H_SF16, 0, 0,
                  DataType::SNorm),
        TEST_CASE(TextureType::BC6, DXGI_FORMAT_BC6H_UF16, DXGI_FORMAT_BC6H_UF16, 0, 0,
                  DataType::Float),
        TEST_CASE(TextureType::BC6, DXGI_FORMAT_BC6H_SF16, DXGI_FORMAT_BC6H_SF16, 0, 0,
                  DataType::SNorm),

        TEST_CASE(TextureType::BC7, DXGI_FORMAT_BC7_TYPELESS, DXGI_FORMAT_BC7_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC7, DXGI_FORMAT_BC7_TYPELESS, DXGI_FORMAT_BC7_UNORM_SRGB, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC7, DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::BC7, DXGI_FORMAT_BC7_UNORM_SRGB, DXGI_FORMAT_BC7_UNORM_SRGB, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::R9G9B9E5, DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
                  DXGI_FORMAT_R9G9B9E5_SHAREDEXP, 0, 0, DataType::Float),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G6R5_UNORM, 0, 0,
                  DataType::UNorm),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM, 0,
                  0, DataType::UNorm),
        TEST_CASE(TextureType::R4G4B4A4, DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, 0,
                  0, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R1_UNORM, DXGI_FORMAT_R1_UNORM, 0, 0,
                  DataType::UNorm),

        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R10G10B10A2_TYPELESS,
                  DXGI_FORMAT_R10G10B10A2_UNORM, 1, 4, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R10G10B10A2_TYPELESS,
                  DXGI_FORMAT_R10G10B10A2_UINT, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R10G10B10A2_UNORM,
                  DXGI_FORMAT_R10G10B10A2_UNORM, 1, 4, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R10G10B10A2_UINT, DXGI_FORMAT_R10G10B10A2_UINT,
                  1, 4, DataType::UInt),

        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT, 0,
                  0, DataType::Float),
    };

    for(D3D11Format f : color_tests)
      AddSupportedTests(f, test_textures, false);

    // finally add the depth tests
    const D3D11Format depth_tests[] = {
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R32G8X24_TYPELESS,
                  DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, 0, 0, DataType::Float),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R32G8X24_TYPELESS,
                  DXGI_FORMAT_X32_TYPELESS_G8X24_UINT, 0, 0, DataType::UInt),

        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R24G8_TYPELESS,
                  DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R24G8_TYPELESS,
                  DXGI_FORMAT_X24_TYPELESS_G8_UINT, 0, 0, DataType::UInt),

        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, 0, 0,
                  DataType::Float),

        TEST_CASE(TextureType::Unknown, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, 0, 0,
                  DataType::UNorm),
    };

    for(D3D11Format f : depth_tests)
      AddSupportedTests(f, test_textures, true);

    popMarker();

    ID3D11Texture2DPtr fltTex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight).RTV().SRV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);
    ID3D11ShaderResourceViewPtr fltSRV = MakeSRV(fltTex);

    ID3D11BufferPtr mscb = MakeBuffer().Constant().Size(sizeof(Vec4i));
    ID3D11VertexShaderPtr msvs = CreateVS(Compile(D3DFullscreenQuadVertex, "main", "vs_4_0"));
    ID3D11PixelShaderPtr msps[(size_t)DataType::Count];

    std::string def = "#define TEX_WIDTH " + std::to_string(texWidth) + "\n\n";

    msps[(size_t)DataType::Float] = msps[(size_t)DataType::UNorm] = msps[(size_t)DataType::SNorm] =
        CreatePS(Compile(def + pixelMSFloat, "main", "ps_5_0"));
    msps[(size_t)DataType::UInt] = CreatePS(Compile(def + pixelMSUInt, "main", "ps_5_0"));
    msps[(size_t)DataType::SInt] = CreatePS(Compile(def + pixelMSSInt, "main", "ps_5_0"));

    ID3D11PixelShaderPtr msdepthps = CreatePS(Compile(def + pixelMSDepth, "main", "ps_5_0"));

    D3D11_DEPTH_STENCIL_DESC ds = GetDepthState();
    ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    ds.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;

    for(TestCase &t : test_textures)
    {
      if(!t.res || t.hasData)
        continue;

      if(!t.canRender && !t.isDepth)
      {
        TEST_ERROR("Need data for test %s, but it's not a renderable/depthable format",
                   t.fmt.name.c_str());
        continue;
      }

      ds.DepthEnable = t.isDepth;
      ds.StencilEnable = t.isDepth;
      SetDepthState(ds);

      ID3D11ResourcePtr res;
      t.srv->GetResource(&res);

      ID3D11Texture1DPtr tex1 = res;
      ID3D11Texture2DPtr tex2 = res;
      ID3D11Texture3DPtr tex3 = res;

      UINT ArraySize = 1, MipLevels = 1, SampleCount = 1;

      if(tex1)
      {
        D3D11_TEXTURE1D_DESC desc;
        tex1->GetDesc(&desc);
        ArraySize = desc.ArraySize;
        MipLevels = desc.MipLevels;
      }
      if(tex2)
      {
        D3D11_TEXTURE2D_DESC desc;
        tex2->GetDesc(&desc);
        ArraySize = desc.ArraySize;
        MipLevels = desc.MipLevels;
        SampleCount = desc.SampleDesc.Count;
      }
      if(tex3)
      {
        D3D11_TEXTURE3D_DESC desc;
        tex3->GetDesc(&desc);
        MipLevels = desc.MipLevels;
        ArraySize = desc.Depth;
      }

      pushMarker("Render data for " + t.fmt.name + " " + MakeName(t));

      t.hasData = true;

      bool srgb = false, bgra = false;
      switch(t.fmt.viewFmt)
      {
        // only need to handle renderable BGRA/SRGB formats here
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B4G4R4A4_UNORM: bgra = true; break;

        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
          bgra = true;
          srgb = true;
          break;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: srgb = true; break;
        default: break;
      }

      int flags = 0;

      if(t.fmt.cfg.data == DataType::SNorm)
        flags |= 1;
      if(srgb)
        flags |= 2;
      if(bgra)
        flags |= 4;
      if(t.fmt.viewFmt == DXGI_FORMAT_A8_UNORM)
        flags |= 8;

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      RSSetViewport({0.0f, 0.0f, (float)texWidth, (float)texHeight, 0.0f, 1.0f});

      for(UINT mp = 0; mp < MipLevels; mp++)
      {
        UINT SlicesOrDepth = ArraySize;
        if(tex3)
          SlicesOrDepth >>= mp;
        for(UINT sl = 0; sl < SlicesOrDepth; sl++)
        {
          if(t.isDepth)
          {
            ID3D11DepthStencilViewPtr dsv =
                tex1 ? MakeDSV(tex1).Format(GetDepthFormat(t.fmt)).FirstSlice(sl).FirstMip(mp)
                     : MakeDSV(tex2).Format(GetDepthFormat(t.fmt)).FirstSlice(sl).FirstMip(mp);

            ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0, 0);

            ctx->VSSetShader(msvs, NULL, 0);
            ctx->PSSetShader(msdepthps, NULL, 0);
            ctx->PSSetConstantBuffers(0, 1, &mscb.GetInterfacePtr());

            ctx->OMSetRenderTargets(0, NULL, dsv);

            // need to do each sample separately to let us vary the stencil value
            for(UINT sm = 0; sm < SampleCount; sm++)
            {
              Vec4i params(tex3 ? 0 : sl, mp, 0, tex3 ? sl : 0);
              ctx->UpdateSubresource(mscb, 0, NULL, &params, sizeof(params), sizeof(params));

              ctx->OMSetBlendState(NULL, NULL, 1 << sm);

              SetStencilRef(100 + (mp + sm) * 10);

              ctx->Draw(4, 0);

              // clip off the diagonal
              params.z = 1;
              ctx->UpdateSubresource(mscb, 0, NULL, &params, sizeof(params), sizeof(params));

              SetStencilRef(10 + (mp + sm) * 10);

              ctx->Draw(4, 0);
            }
          }
          else
          {
            ID3D11RenderTargetViewPtr rtv =
                tex1   ? MakeRTV(tex1).Format(t.fmt.viewFmt).FirstSlice(sl).FirstMip(mp)
                : tex2 ? MakeRTV(tex2).Format(t.fmt.viewFmt).FirstSlice(sl).FirstMip(mp)
                       : MakeRTV(tex3).Format(t.fmt.viewFmt).FirstSlice(sl).FirstMip(mp);

            Vec4i params(tex3 ? 0 : sl, mp, flags, tex3 ? sl : 0);
            ctx->UpdateSubresource(mscb, 0, NULL, &params, sizeof(params), sizeof(params));

            ctx->VSSetShader(msvs, NULL, 0);
            ctx->PSSetShader(msps[(size_t)t.fmt.cfg.data], NULL, 0);
            ctx->PSSetConstantBuffers(0, 1, &mscb.GetInterfacePtr());

            ctx->OMSetRenderTargets(1, &rtv.GetInterfacePtr(), NULL);

            ctx->Draw(4, 0);
          }
        }
      }

      popMarker();
    }

    ds.DepthEnable = FALSE;
    ds.StencilEnable = FALSE;
    SetDepthState(ds);

    std::vector<Vec4f> blue;
    blue.resize(64 * 64 * 64, Vec4f(0.0f, 0.0f, 1.0f, 1.0f));

    std::vector<Vec4f> green;
    green.resize(64 * 64, Vec4f(0.0f, 1.0f, 0.0f, 1.0f));

    // slice testing textures

    TestCase slice_test_array = {};
    TestCase slice_test_3d = {};
    slice_test_array.res =
        (ID3D11Texture2DPtr)MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).Array(64).Mips(2).SRV();
    slice_test_array.srv = MakeSRV((ID3D11Texture2DPtr)slice_test_array.res);
    slice_test_array.dim = 2;
    slice_test_array.isArray = true;

    for(UINT slice = 0; slice < 64; slice++)
    {
      ctx->UpdateSubresource(slice_test_array.res, slice * 2, NULL,
                             slice == 17 ? green.data() : blue.data(), 64 * 4, 64 * 64 * 4);
      ctx->UpdateSubresource(slice_test_array.res, slice * 2 + 1, NULL,
                             slice == 17 ? green.data() : blue.data(), 32 * 4, 32 * 32 * 4);
    }

    slice_test_3d.res =
        (ID3D11Texture3DPtr)MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64, 64).Mips(2).SRV();
    slice_test_3d.srv = MakeSRV((ID3D11Texture3DPtr)slice_test_3d.res);
    slice_test_3d.dim = 3;

    ctx->UpdateSubresource(slice_test_3d.res, 0, NULL, blue.data(), 64 * 4, 64 * 64 * 4);
    ctx->UpdateSubresource(slice_test_3d.res, 1, NULL, blue.data(), 32 * 4, 32 * 32 * 4);

    D3D11_BOX box = {};
    box.right = box.bottom = 64;
    box.front = 17;
    box.back = 18;
    ctx->UpdateSubresource(slice_test_3d.res, 0, &box, green.data(), 64 * 4, 64 * 64 * 4);
    box.right = box.bottom = 32;
    ctx->UpdateSubresource(slice_test_3d.res, 1, &box, green.data(), 32 * 4, 32 * 32 * 4);

    while(Running())
    {
      ctx->ClearState();
      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      ctx->VSSetShader(vs, NULL, 0);

      ctx->OMSetRenderTargets(1, &fltRT.GetInterfacePtr(), NULL);

      D3D11_VIEWPORT view = {0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f};

      D3D11_RASTERIZER_DESC rs = GetRasterState();
      rs.ScissorEnable = TRUE;
      SetRasterState(rs);

      RSSetViewport(view);

      // dummy draw for each slice test texture
      pushMarker("slice tests");
      setMarker("2D array");
      ctx->PSSetShader(GetShader(slice_test_array), NULL, 0);
      ctx->PSSetShaderResources(0, 1, &slice_test_array.srv.GetInterfacePtr());
      ctx->Draw(0, 0);

      setMarker("3D");
      ctx->PSSetShader(GetShader(slice_test_3d), NULL, 0);
      ctx->PSSetShaderResources(0, 1, &slice_test_3d.srv.GetInterfacePtr());
      ctx->Draw(0, 0);
      popMarker();

      for(size_t i = 0; i < test_textures.size(); i++)
      {
        if(i == 0 || test_textures[i].fmt.texFmt != test_textures[i - 1].fmt.texFmt ||
           test_textures[i].fmt.viewFmt != test_textures[i - 1].fmt.viewFmt)
        {
          if(i != 0)
            popMarker();

          pushMarker(test_textures[i].fmt.name);
        }

        setMarker(MakeName(test_textures[i]));

        RSSetViewport(view);
        D3D11_RECT rect = {
            (LONG)view.TopLeftX,
            (LONG)view.TopLeftY,
            LONG(view.TopLeftX + view.Width),
            LONG(view.TopLeftY + view.Height),
        };
        rect.left++;
        rect.top++;
        rect.right--;
        rect.bottom--;
        ctx->RSSetScissorRects(1, &rect);
        ctx->PSSetShader(GetShader(test_textures[i]), NULL, 0);
        ctx->PSSetShaderResources(0, 1, &test_textures[i].srv.GetInterfacePtr());

        if(test_textures[i].srv)
          ctx->Draw(4, 0);
        else
          setMarker("UNSUPPORTED");

        // advance to next viewport
        view.TopLeftX += view.Width;
        if(view.TopLeftX + view.Width > (float)screenWidth)
        {
          view.TopLeftX = 0;
          view.TopLeftY += view.Height;
        }
      }

      // pop the last format region
      popMarker();

      rs.ScissorEnable = FALSE;
      SetRasterState(rs);

      // blit to the screen for a nicer preview
      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);
      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      ctx->PSSetShader(blitps, NULL, 0);
      ctx->PSSetShaderResources(0, 1, &fltSRV.GetInterfacePtr());
      ctx->Draw(4, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
