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

#include "d3d12_test.h"

using namespace TextureZoo;

RD_TEST(D3D12_Texture_Zoo, D3D12GraphicsTest)
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

  struct D3D12Format
  {
    const std::string name;
    DXGI_FORMAT texFmt;
    DXGI_FORMAT viewFmt;
    TexConfig cfg;
  };

  struct TestCase
  {
    D3D12Format fmt;
    uint32_t dim;
    bool isArray;
    bool canRender;
    bool isDepth;
    bool isMSAA;
    bool hasData;
    ID3D12ResourcePtr res;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
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

  ID3DBlobPtr vsblob;
  ID3D12RootSignaturePtr sig;

  ID3D12PipelineStatePtr GetPSO(const TestCase &test)
  {
    static std::map<uint32_t, ID3D12PipelineStatePtr> PSOs;

    bool isStencilOut = (test.fmt.viewFmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
                         test.fmt.viewFmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT);

    uint32_t key = uint32_t(test.fmt.cfg.data);
    key |= test.dim << 6;
    key |= test.isMSAA ? 0x80000 : 0;
    key |= isStencilOut ? 0x100000 : 0;

    ID3D12PipelineStatePtr ret = PSOs[key];
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

      ID3DBlobPtr psblob = Compile(src, "main", "ps_5_0");
      ret = PSOs[key] =
          MakePSO().RootSig(sig).VS(vsblob).PS(psblob).RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    }

    return ret;
  }

  uint32_t srvIndex = 0;
  UINT64 CurOffset = 0;
  ID3D12ResourcePtr uploadBuf;
  byte *CurBuf = NULL;

  bool SetData(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr res, const D3D12Format &fmt)
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

        if(s == 0 && m == 0)
          ResourceBarrier(cmd, res, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        D3D12_TEXTURE_COPY_LOCATION src = {};

        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = res;
        dst.SubresourceIndex = s * mips + m;

        TEST_ASSERT(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT >= data.rowPitch,
                    "Row pitch higher than alignment!");

        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.pResource = uploadBuf;
        src.PlacedFootprint.Offset = CurOffset;
        src.PlacedFootprint.Footprint.Format = res->GetDesc().Format;
        src.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        src.PlacedFootprint.Footprint.Width = std::max(1, dim.x >> m);
        src.PlacedFootprint.Footprint.Height = std::max(1, dim.y >> m);
        src.PlacedFootprint.Footprint.Depth = std::max(1, dim.z >> m);

        bool block = false;
        switch(src.PlacedFootprint.Footprint.Format)
        {
          case DXGI_FORMAT_BC1_TYPELESS:
          case DXGI_FORMAT_BC1_UNORM:
          case DXGI_FORMAT_BC1_UNORM_SRGB:
          case DXGI_FORMAT_BC4_TYPELESS:
          case DXGI_FORMAT_BC4_UNORM:
          case DXGI_FORMAT_BC4_SNORM:
          case DXGI_FORMAT_BC2_TYPELESS:
          case DXGI_FORMAT_BC2_UNORM:
          case DXGI_FORMAT_BC2_UNORM_SRGB:
          case DXGI_FORMAT_BC3_TYPELESS:
          case DXGI_FORMAT_BC3_UNORM:
          case DXGI_FORMAT_BC3_UNORM_SRGB:
          case DXGI_FORMAT_BC5_TYPELESS:
          case DXGI_FORMAT_BC5_UNORM:
          case DXGI_FORMAT_BC5_SNORM:
          case DXGI_FORMAT_BC6H_TYPELESS:
          case DXGI_FORMAT_BC6H_UF16:
          case DXGI_FORMAT_BC6H_SF16:
          case DXGI_FORMAT_BC7_TYPELESS:
          case DXGI_FORMAT_BC7_UNORM:
          case DXGI_FORMAT_BC7_UNORM_SRGB:
            src.PlacedFootprint.Footprint.Width = AlignUp(src.PlacedFootprint.Footprint.Width, 4U);
            src.PlacedFootprint.Footprint.Height = AlignUp(src.PlacedFootprint.Footprint.Height, 4U);
            block = true;
            break;
        }

        UINT numRows = src.PlacedFootprint.Footprint.Height * src.PlacedFootprint.Footprint.Depth;

        if(block)
          numRows /= 4U;

        for(UINT r = 0; r < numRows; r++)
        {
          memcpy(CurBuf + CurOffset + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * r,
                 data.byteData.data() + data.rowPitch * r, data.rowPitch);
        }

        CurOffset += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * numRows;

        CurOffset = AlignUp(CurOffset, (UINT64)D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
      }
    }

    ResourceBarrier(cmd, res, D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return true;
  }

  void GetDimensions(ID3D12ResourcePtr res, Vec4i & dim, UINT & mips, UINT & slices)
  {
    D3D12_RESOURCE_DESC desc = res->GetDesc();

    dim.x = (int)desc.Width;
    dim.y = desc.Height;

    mips = std::max(1, (int)desc.MipLevels);

    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
      dim.z = desc.DepthOrArraySize;
      slices = 1;
    }
    else
    {
      dim.z = 1;
      slices = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
                   ? 1
                   : std::max(1, (int)desc.DepthOrArraySize);
    }
  }

  TestCase FinaliseTest(ID3D12GraphicsCommandListPtr cmd, TestCase test)
  {
    UINT planeSlice = 0;

    if(test.fmt.viewFmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
       test.fmt.viewFmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
      planeSlice = 1;

    if(test.dim == 1)
    {
      D3D12TextureCreator creator =
          MakeTexture(test.fmt.texFmt, texWidth).Mips(texMips).Array(test.isArray ? texSlices : 1);

      if(test.isDepth)
        creator.DSV();
      else if(test.canRender)
        creator.RTV();

      test.res = creator;
      test.srv =
          MakeSRV(test.res).PlaneSlice(planeSlice).Format(test.fmt.viewFmt).CreateGPU(srvIndex++);
    }
    else if(test.dim == 2 && !test.isMSAA)
    {
      D3D12TextureCreator creator = MakeTexture(test.fmt.texFmt, texWidth, texHeight)
                                        .Mips(texMips)
                                        .Array(test.isArray ? texSlices : 1);

      if(test.isDepth)
        creator.DSV();
      else if(test.canRender)
        creator.RTV();

      test.res = creator;
      test.srv =
          MakeSRV(test.res).PlaneSlice(planeSlice).Format(test.fmt.viewFmt).CreateGPU(srvIndex++);
    }
    else if(test.dim == 2 && test.isMSAA)
    {
      D3D12TextureCreator creator = MakeTexture(test.fmt.texFmt, texWidth, texHeight)
                                        .Multisampled(texSamples)
                                        .Array(test.isArray ? texSlices : 1);

      if(test.isDepth)
        creator.DSV();
      else
        creator.RTV();

      test.res = creator;
      test.srv =
          MakeSRV(test.res).PlaneSlice(planeSlice).Format(test.fmt.viewFmt).CreateGPU(srvIndex++);
      test.canRender = true;
    }
    else if(test.dim == 3)
    {
      D3D12TextureCreator creator =
          MakeTexture(test.fmt.texFmt, texWidth, texHeight, texDepth).Mips(texMips);

      if(test.canRender)
        creator.RTV();

      test.res = creator;
      test.srv =
          MakeSRV(test.res).PlaneSlice(planeSlice).Format(test.fmt.viewFmt).CreateGPU(srvIndex++);
    }

    test.res->SetName(UTF82Wide(MakeName(test) + " " + test.fmt.name).c_str());

    if(!test.isMSAA)
    {
      pushMarker(cmd, "Set data for " + test.fmt.name + " " + MakeName(test));

      test.hasData = SetData(cmd, test.res, test.fmt);

      popMarker(cmd);
    }

    return test;
  }

  DXGI_FORMAT GetDepthFormat(const D3D12Format &f)
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

  void AddSupportedTests(const D3D12Format &f, std::vector<TestCase> &test_textures, bool depthMode)
  {
    DXGI_FORMAT queryFormat = f.viewFmt;

    ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

    Reset(cmd);

    CurOffset = 0;

    if(depthMode)
      queryFormat = GetDepthFormat(f);

    D3D12_FEATURE_DATA_FORMAT_SUPPORT supp = {};
    supp.Format = queryFormat;
    dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &supp, sizeof(supp));

    bool renderable = (supp.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0;
    bool depth = (supp.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) != 0;

    if((supp.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD) || depth)
    {
      // TODO: disable 1D depth textures for now, we don't support displaying them
      if(!depthMode)
      {
        if(supp.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE1D)
        {
          test_textures.push_back(FinaliseTest(cmd, {f, 1, false, renderable, depth}));
          test_textures.push_back(FinaliseTest(cmd, {f, 1, true, renderable, depth}));
        }
        else
        {
          test_textures.push_back({f, 1, false});
          test_textures.push_back({f, 1, true});
        }
      }

      if(supp.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D)
      {
        test_textures.push_back(FinaliseTest(cmd, {f, 2, false, renderable, depth}));
        test_textures.push_back(FinaliseTest(cmd, {f, 2, true, renderable, depth}));
      }
      else
      {
        test_textures.push_back({f, 2, false});
        test_textures.push_back({f, 2, true});
      }
      if(supp.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE3D)
      {
        test_textures.push_back(FinaliseTest(cmd, {f, 3, false, renderable, depth}));
      }
      else
      {
        test_textures.push_back({f, 3, false});
      }
      if(((supp.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD) || depth) &&
         (supp.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET))
      {
        test_textures.push_back(FinaliseTest(cmd, {f, 2, false, true, depth, true}));
        test_textures.push_back(FinaliseTest(cmd, {f, 2, true, true, depth, true}));
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

      if(supp.Support1 & (D3D12_FORMAT_SUPPORT1_TEXTURE1D | D3D12_FORMAT_SUPPORT1_TEXTURE2D |
                          D3D12_FORMAT_SUPPORT1_TEXTURE3D))
      {
        TEST_ERROR("Format %d can't be loaded in shader but can be a texture!", f.texFmt);
      }
    }

    cmd->Close();

    Submit({cmd});

    GPUSync();
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    vsblob = Compile(D3DFullscreenQuadVertex, "main", "vs_4_0");

    sig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 4),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1),
    });

    ID3DBlobPtr psblob = Compile(pixelBlit, "main", "ps_5_0");

    ID3D12PipelineStatePtr blitpso = MakePSO().RootSig(sig).VS(vsblob).PS(psblob);

    uploadBuf = MakeBuffer().Upload().Size(8 * 1024 * 1024);

    CurBuf = Map(uploadBuf, 0);

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

    const D3D12Format color_tests[] = {
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

    for(D3D12Format f : color_tests)
      AddSupportedTests(f, test_textures, false);

    // finally add the depth tests
    const D3D12Format depth_tests[] = {
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

    for(D3D12Format f : depth_tests)
      AddSupportedTests(f, test_textures, true);

    uploadBuf->Unmap(0, NULL);

    ID3D12ResourcePtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_GPU_DESCRIPTOR_HANDLE fltSRV = MakeSRV(fltTex).CreateGPU(srvIndex++);

    ID3DBlobPtr msps[(size_t)DataType::Count];

    std::string def = "#define TEX_WIDTH " + std::to_string(texWidth) + "\n\n";

    msps[(size_t)DataType::Float] = msps[(size_t)DataType::UNorm] = msps[(size_t)DataType::SNorm] =
        Compile(def + pixelMSFloat, "main", "ps_5_0");
    msps[(size_t)DataType::UInt] = Compile(def + pixelMSUInt, "main", "ps_5_0");
    msps[(size_t)DataType::SInt] = Compile(def + pixelMSSInt, "main", "ps_5_0");

    ID3DBlobPtr msdepthps = Compile(def + pixelMSDepth, "main", "ps_5_0");

    D3D12PSOCreator psoCreator = MakePSO().RootSig(sig).VS(vsblob);

    psoCreator.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoCreator.GraphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoCreator.GraphicsDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;

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

      psoCreator.GraphicsDesc.DepthStencilState.DepthEnable = t.isDepth;
      psoCreator.GraphicsDesc.DepthStencilState.StencilEnable = t.isDepth;

      D3D12_RESOURCE_DESC desc = t.res->GetDesc();

      bool tex3d = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;

      UINT MipLevels = desc.MipLevels, SampleCount = desc.SampleDesc.Count;

      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      pushMarker(cmd, "Render data for " + t.fmt.name + " " + MakeName(t));

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

      D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

      if(t.isDepth)
      {
        psoCreator.PS(msdepthps).DSV(GetDepthFormat(t.fmt));

        state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      }
      else
      {
        psoCreator.PS(msps[(size_t)t.fmt.cfg.data]).RTVs({t.fmt.viewFmt});

        state = D3D12_RESOURCE_STATE_RENDER_TARGET;
      }

      ResourceBarrier(cmd, t.res, D3D12_RESOURCE_STATE_COMMON, state);

      psoCreator.SampleCount(SampleCount);

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      cmd->SetGraphicsRootSignature(sig);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      RSSetViewport(cmd, {0.0f, 0.0f, (float)texWidth, (float)texHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, texWidth, texHeight});

      // keep all PSOs alive until the end of the loop where we submit the command buffer
      std::vector<ID3D12PipelineStatePtr> PSOs;

      for(UINT mp = 0; mp < MipLevels; mp++)
      {
        UINT SlicesOrDepth = desc.DepthOrArraySize;
        if(tex3d)
          SlicesOrDepth >>= mp;

        for(UINT sl = 0; sl < SlicesOrDepth; sl++)
        {
          if(t.isDepth)
          {
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = MakeDSV(t.res)
                                                  .Format(GetDepthFormat(t.fmt))
                                                  .FirstSlice(sl)
                                                  .NumSlices(1)
                                                  .FirstMip(mp)
                                                  .NumMips(1)
                                                  .CreateCPU(0);

            D3D12_RECT rect = {};
            rect.right = (LONG)std::max(1ULL, desc.Width >> mp);
            rect.bottom = (LONG)std::max(1U, desc.Height >> mp);

            cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0,
                                       0, 1, &rect);

            OMSetRenderTargets(cmd, {}, dsv);

            // need to do each sample separately to let us vary the stencil value
            for(UINT sm = 0; sm < SampleCount; sm++)
            {
              psoCreator.GraphicsDesc.SampleMask = 1 << sm;

              ID3D12PipelineStatePtr pso = psoCreator;
              PSOs.push_back(pso);

              cmd->SetPipelineState(pso);

              Vec4i params(tex3d ? 0 : sl, mp, 0, tex3d ? sl : 0);
              cmd->SetGraphicsRoot32BitConstants(0, 4, &params, 0);

              cmd->OMSetStencilRef(100 + (mp + sm) * 10);

              cmd->DrawInstanced(4, 1, 0, 0);

              // clip off the diagonal
              params.z = 1;
              cmd->SetGraphicsRoot32BitConstants(0, 4, &params, 0);

              cmd->OMSetStencilRef(10 + (mp + sm) * 10);

              cmd->DrawInstanced(4, 1, 0, 0);
            }
          }
          else
          {
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = MakeRTV(t.res)
                                                  .Format(t.fmt.viewFmt)
                                                  .FirstSlice(sl)
                                                  .NumSlices(1)
                                                  .FirstMip(mp)
                                                  .NumMips(1)
                                                  .CreateCPU(0);

            ID3D12PipelineStatePtr pso = psoCreator;
            PSOs.push_back(pso);

            cmd->SetPipelineState(pso);

            Vec4i params(tex3d ? 0 : sl, mp, flags, tex3d ? sl : 0);
            cmd->SetGraphicsRoot32BitConstants(0, 4, &params, 0);

            OMSetRenderTargets(cmd, {rtv}, {});

            cmd->DrawInstanced(4, 1, 0, 0);
          }
        }
      }

      ResourceBarrier(cmd, t.res, state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

      popMarker(cmd);

      cmd->Close();

      Submit({cmd});

      GPUSync();
    }

    std::vector<Vec4f> blue;
    blue.resize(64 * 64 * 64, Vec4f(0.0f, 0.0f, 1.0f, 1.0f));

    std::vector<Vec4f> green;
    green.resize(64 * 64, Vec4f(0.0f, 1.0f, 0.0f, 1.0f));

    CurBuf = Map(uploadBuf, 0);

    memcpy(CurBuf, blue.data(), blue.size() * sizeof(Vec4f));
    memcpy(CurBuf + blue.size() * sizeof(Vec4f), green.data(), green.size() * sizeof(Vec4f));

    uploadBuf->Unmap(0, NULL);

    // slice testing textures

    TestCase slice_test_array = {};
    TestCase slice_test_3d = {};
    slice_test_array.dim = 2;
    slice_test_array.isArray = true;
    slice_test_array.res = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64).Array(64).Mips(2);
    slice_test_array.srv = MakeSRV(slice_test_array.res).CreateGPU(srvIndex++);

    slice_test_3d.dim = 3;
    slice_test_3d.res = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64, 64).Mips(2);
    slice_test_3d.srv = MakeSRV(slice_test_3d.res).CreateGPU(srvIndex++);

    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ResourceBarrier(cmd, slice_test_array.res, D3D12_RESOURCE_STATE_COMMON,
                      D3D12_RESOURCE_STATE_COPY_DEST);
      ResourceBarrier(cmd, slice_test_3d.res, D3D12_RESOURCE_STATE_COMMON,
                      D3D12_RESOURCE_STATE_COPY_DEST);

      for(UINT s = 0; s < 64; s++)
      {
        for(UINT m = 0; m < 2; m++)
        {
          D3D12_TEXTURE_COPY_LOCATION dst = {};
          D3D12_TEXTURE_COPY_LOCATION src = {};

          dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
          dst.pResource = slice_test_array.res;
          dst.SubresourceIndex = s * 2 + m;

          src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          src.pResource = uploadBuf;
          src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
          src.PlacedFootprint.Footprint.RowPitch = (64 >> m) * sizeof(Vec4f);
          src.PlacedFootprint.Footprint.Width = 64 >> m;
          src.PlacedFootprint.Footprint.Height = 64 >> m;
          src.PlacedFootprint.Footprint.Depth = 1;

          if(s == 17)
            src.PlacedFootprint.Offset = blue.size() * sizeof(Vec4f);

          cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

          if(s == 0)
          {
            dst.pResource = slice_test_3d.res;
            dst.SubresourceIndex = m;
            src.PlacedFootprint.Footprint.Depth = 64 >> m;

            cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
          }
        }
      }

      for(int m = 0; m < 2; m++)
      {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        D3D12_TEXTURE_COPY_LOCATION src = {};

        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = slice_test_3d.res;
        dst.SubresourceIndex = m;

        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.pResource = uploadBuf;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        src.PlacedFootprint.Footprint.RowPitch = (64 >> m) * sizeof(Vec4f);
        src.PlacedFootprint.Footprint.Width = 64 >> m;
        src.PlacedFootprint.Footprint.Height = 64 >> m;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Offset = blue.size() * sizeof(Vec4f);

        cmd->CopyTextureRegion(&dst, 0, 0, 17, &src, NULL);
      }

      ResourceBarrier(cmd, slice_test_array.res, D3D12_RESOURCE_STATE_COPY_DEST,
                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      ResourceBarrier(cmd, slice_test_3d.res, D3D12_RESOURCE_STATE_COPY_DEST,
                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

      cmd->Close();

      Submit({cmd});

      GPUSync();
    }

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      D3D12_CPU_DESCRIPTOR_HANDLE fltRTV = MakeRTV(fltTex).CreateCPU(0);

      ClearRenderTargetView(cmd, fltRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      OMSetRenderTargets(cmd, {fltRTV}, {});

      cmd->SetGraphicsRootSignature(sig);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      D3D12_VIEWPORT view = {0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 1.0f};

      {
        RSSetViewport(cmd, view);
        D3D12_RECT rect = {
            (LONG)view.TopLeftX,
            (LONG)view.TopLeftY,
            LONG(view.TopLeftX + view.Width),
            LONG(view.TopLeftY + view.Height),
        };
        RSSetScissorRect(cmd, rect);
      }

      // dummy draw for each slice test texture
      pushMarker(cmd, "slice tests");
      setMarker(cmd, "2D array");
      cmd->SetPipelineState(GetPSO(slice_test_array));
      cmd->SetGraphicsRootDescriptorTable(1, slice_test_array.srv);
      cmd->DrawInstanced(0, 0, 0, 0);

      setMarker(cmd, "3D");
      cmd->SetPipelineState(GetPSO(slice_test_3d));
      cmd->SetGraphicsRootDescriptorTable(1, slice_test_3d.srv);
      cmd->DrawInstanced(0, 0, 0, 0);
      popMarker(cmd);

      for(size_t i = 0; i < test_textures.size(); i++)
      {
        if(i == 0 || test_textures[i].fmt.texFmt != test_textures[i - 1].fmt.texFmt ||
           test_textures[i].fmt.viewFmt != test_textures[i - 1].fmt.viewFmt)
        {
          if(i != 0)
            popMarker(cmd);

          pushMarker(cmd, test_textures[i].fmt.name);
        }

        setMarker(cmd, MakeName(test_textures[i]));

        RSSetViewport(cmd, view);
        D3D12_RECT rect = {
            (LONG)view.TopLeftX,
            (LONG)view.TopLeftY,
            LONG(view.TopLeftX + view.Width),
            LONG(view.TopLeftY + view.Height),
        };
        rect.left++;
        rect.top++;
        rect.right--;
        rect.bottom--;
        RSSetScissorRect(cmd, rect);

        cmd->SetPipelineState(GetPSO(test_textures[i]));

        if(test_textures[i].srv.ptr != 0)
        {
          cmd->SetGraphicsRootDescriptorTable(1, test_textures[i].srv);
          cmd->DrawInstanced(4, 1, 0, 0);
        }
        else
        {
          setMarker(cmd, "UNSUPPORTED");
        }

        // advance to next viewport
        view.TopLeftX += view.Width;
        if(view.TopLeftX + view.Width > (float)screenWidth)
        {
          view.TopLeftX = 0;
          view.TopLeftY += view.Height;
        }
      }

      // pop the last format region
      popMarker(cmd);

      ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      OMSetRenderTargets(cmd, {rtv}, {});

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      cmd->SetPipelineState(blitpso);
      cmd->SetGraphicsRootDescriptorTable(1, fltSRV);
      cmd->DrawInstanced(4, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      ResourceBarrier(cmd, fltTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
