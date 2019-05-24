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

#include "d3d12_test.h"

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
//                          **** WARNING ****                                    //
//                                                                               //
// When comparing to Vulkan tests, the order of channels in the data is *not*    //
// necessarily the same - vulkan expects Y in G, Cb/U in B and Cr/V in R         //
// consistently, where some of the D3D formats are a bit different.              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

TEST(D3D12_Video_Textures, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests of YUV textures";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

#define MODE_RGB 0
#define MODE_YUV_DEFAULT 1

cbuffer cb : register(b0)
{
  int2 dimensions;
  uint2 downsampling;
  int y_channel;
  int u_channel;
  int v_channel;
  int mode;
};

Texture2D<float4> tex : register(t0);
Texture2D<float4> tex2 : register(t1);

float4 main(v2f IN) : SV_Target0
{
  uint3 coord = uint3(IN.uv.xy * float2(dimensions.xy), 0);

  bool use_second_y = false;

  // detect interleaved 4:2:2.
  // 4:2:0 will have downsampling.x == downsampling.y == 2,
  // 4:4:4 will have downsampling.x == downsampling.y == 1
  // planar formats will have one one channel >= 4 i.e. in the second texture.
  if(downsampling.x > downsampling.y && y_channel < 4 && u_channel < 4 && v_channel < 4)
  {
    // if we're in an odd pixel, use second Y sample. See below
    use_second_y = ((coord.x & 1u) != 0);
    // downsample co-ordinates
    coord.xy /= downsampling.xy;
  }

	float4 texvec = tex.Load(coord);

  // if we've sampled interleaved YUYV, for odd x co-ords we use .z for luma
  if(use_second_y)
    texvec.x = texvec.z;

  if(mode == MODE_RGB) return texvec;

  coord = uint3(IN.uv.xy * float2(dimensions.xy), 0);

  // downsample co-ordinates for second texture
  coord.xy /= downsampling.xy;

	float4 texvec2 = tex2.Load(coord);

  float texdata[] = {
    texvec.x,  texvec.y,  texvec.z,  texvec.w,
    texvec2.x, texvec2.y, texvec2.z, texvec2.w,
  };

  float Y = texdata[y_channel];
  float U = texdata[u_channel];
  float V = texdata[v_channel];
  float A = float(texvec.w);

  const float Kr = 0.2126f;
  const float Kb = 0.0722f;

  float L = Y;
  float Pb = U - 0.5f;
  float Pr = V - 0.5f;

  // these are just reversals of the equations below

  float B = L + (Pb / 0.5f) * (1 - Kb);
  float R = L + (Pr / 0.5f) * (1 - Kr);
  float G = (L - Kr * R - Kb * B) / (1.0f - Kr - Kb);

  return float4(R, G, B, A);
}

)EOSHADER";

  struct YUVPixel
  {
    uint16_t Y, Cb, Cr, A;
  };

  // we use a plain un-scaled un-offsetted direct conversion
  YUVPixel RGB2YUV(uint32_t rgba)
  {
    uint32_t r = rgba & 0xff;
    uint32_t g = (rgba >> 8) & 0xff;
    uint32_t b = (rgba >> 16) & 0xff;
    uint16_t a = (rgba >> 24) & 0xff;

    const float Kr = 0.2126f;
    const float Kb = 0.0722f;

    float R = float(r) / 255.0f;
    float G = float(g) / 255.0f;
    float B = float(b) / 255.0f;

    // calculate as floats since we're not concerned with performance here
    float L = Kr * R + Kb * B + (1.0f - Kr - Kb) * G;

    float Pb = ((B - L) / (1 - Kb)) * 0.5f;
    float Pr = ((R - L) / (1 - Kr)) * 0.5f;
    float fA = float(a) / 255.0f;

    uint16_t Y = (uint16_t)(L * 65536.0f);
    uint16_t Cb = (uint16_t)((Pb + 0.5f) * 65536.0f);
    uint16_t Cr = (uint16_t)((Pr + 0.5f) * 65536.0f);
    uint16_t A = (uint16_t)(fA * 65535.0f);

    return {Y, Cb, Cr, A};
  }

  struct TextureData
  {
    ID3D12ResourcePtr tex;
    const char *name;
    D3D12_GPU_DESCRIPTOR_HANDLE views;
    ID3D12ResourcePtr cb;
  };

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    const DefaultA2V verts[4] = {
        {Vec3f(-1.0f, -1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, -1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f)},
        {Vec3f(1.0f, 1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    std::vector<byte> yuv8;
    std::vector<uint16_t> yuv16;
    yuv8.reserve(rgba8.data.size() * 4);
    yuv16.reserve(rgba8.data.size() * 4);

    for(uint32_t y = 0; y < rgba8.height; y++)
    {
      for(uint32_t x = 0; x < rgba8.width; x++)
      {
        YUVPixel p = RGB2YUV(rgba8.data[y * rgba8.width + x]);

        yuv16.push_back(p.Cb);
        yuv16.push_back(p.Y);
        yuv16.push_back(p.Cr);
        yuv16.push_back(p.A);

        yuv8.push_back(p.Cr >> 8);
        yuv8.push_back(p.Cb >> 8);
        yuv8.push_back(p.Y >> 8);
        yuv8.push_back(p.A >> 8);
      }
    }

    UINT reqsupp = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_LOAD;

    TextureData textures[20] = {};
    uint32_t texidx = 0;

    ID3D12ResourcePtr uploadBuf = MakeBuffer().Upload().Size(rgba8.width * rgba8.height * 16);

    auto make_tex = [&](const char *name, uint32_t subsampling, DXGI_FORMAT texFmt,
                        DXGI_FORMAT viewFmt, DXGI_FORMAT view2Fmt, Vec4i config, void *data) {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT supp = {};
      supp.Format = texFmt;
      dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &supp, sizeof(supp));

      {
        TEST_LOG("%s supports:", name);
        if(supp.Support1 == 0)
          TEST_LOG("  - NONE");
#define CHECK_SUPP(s)                           \
  if(supp.Support1 & D3D12_FORMAT_SUPPORT1_##s) \
    TEST_LOG("  - " #s);
        CHECK_SUPP(BUFFER)
        CHECK_SUPP(IA_VERTEX_BUFFER)
        CHECK_SUPP(IA_INDEX_BUFFER)
        CHECK_SUPP(SO_BUFFER)
        CHECK_SUPP(TEXTURE1D)
        CHECK_SUPP(TEXTURE2D)
        CHECK_SUPP(TEXTURE3D)
        CHECK_SUPP(TEXTURECUBE)
        CHECK_SUPP(SHADER_LOAD)
        CHECK_SUPP(SHADER_SAMPLE)
        CHECK_SUPP(SHADER_SAMPLE_COMPARISON)
        CHECK_SUPP(SHADER_SAMPLE_MONO_TEXT)
        CHECK_SUPP(MIP)
        CHECK_SUPP(RENDER_TARGET)
        CHECK_SUPP(BLENDABLE)
        CHECK_SUPP(DEPTH_STENCIL)
        CHECK_SUPP(MULTISAMPLE_RESOLVE)
        CHECK_SUPP(DISPLAY)
        CHECK_SUPP(CAST_WITHIN_BIT_LAYOUT)
        CHECK_SUPP(MULTISAMPLE_RENDERTARGET)
        CHECK_SUPP(MULTISAMPLE_LOAD)
        CHECK_SUPP(SHADER_GATHER)
        CHECK_SUPP(BACK_BUFFER_CAST)
        CHECK_SUPP(TYPED_UNORDERED_ACCESS_VIEW)
        CHECK_SUPP(SHADER_GATHER_COMPARISON)
        CHECK_SUPP(DECODER_OUTPUT)
        CHECK_SUPP(VIDEO_PROCESSOR_OUTPUT)
        CHECK_SUPP(VIDEO_PROCESSOR_INPUT)
        CHECK_SUPP(VIDEO_ENCODER)
      }

      uint32_t horizDownsampleFactor = ((subsampling % 100) / 10);
      uint32_t vertDownsampleFactor = (subsampling % 10);

      // 4:4:4
      if(horizDownsampleFactor == 4 && vertDownsampleFactor == 4)
      {
        horizDownsampleFactor = vertDownsampleFactor = 1;
      }

      // 4:2:2
      else if(horizDownsampleFactor == 2 && vertDownsampleFactor == 2)
      {
        vertDownsampleFactor = 1;
      }

      // 4:2:0
      else if(horizDownsampleFactor == 2 && vertDownsampleFactor == 0)
      {
        vertDownsampleFactor = 2;
      }
      else
      {
        TEST_FATAL("Unhandled subsampling %d", subsampling);
      }

      if((supp.Support1 & reqsupp) == reqsupp)
      {
        ID3D12ResourcePtr tex = MakeTexture(texFmt, rgba8.width, rgba8.height)
                                    .Mips(1)
                                    .InitialState(D3D12_RESOURCE_STATE_COPY_DEST);
        Vec4i cbdata[2] = {
            Vec4i(rgba8.width, rgba8.height, horizDownsampleFactor, vertDownsampleFactor), config,
        };
        ID3D12ResourcePtr cb = MakeBuffer().Data(cbdata);

        D3D12_FEATURE_DATA_FORMAT_INFO info;
        info.Format = texFmt;
        dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &info, sizeof(info));
        UINT numPlanes = info.PlaneCount;

        TEST_ASSERT(numPlanes <= 2, "Don't support 3-plane textures");

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[2] = {};
        UINT numrows[2] = {};
        UINT64 rowsizes[2] = {};
        UINT64 totalbytes = 0;

        D3D12_RESOURCE_DESC desc = tex->GetDesc();

        dev->GetCopyableFootprints(&desc, 0, numPlanes, 0, layouts, numrows, rowsizes, &totalbytes);

        TEST_ASSERT(totalbytes <= rgba8.width * rgba8.height * 16,
                    "Upload buffer is not big enough");

        {
          byte *srcptr = (byte *)data;
          byte *mapptr = NULL;
          uploadBuf->Map(0, NULL, (void **)&mapptr);

          ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

          Reset(cmd);

          for(UINT i = 0; i < numPlanes; i++)
          {
            D3D12_TEXTURE_COPY_LOCATION dst, src;

            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = tex;
            dst.SubresourceIndex = i;

            byte *dstptr = mapptr + layouts[i].Offset;

            for(UINT row = 0; row < numrows[i]; row++)
            {
              memcpy(dstptr, srcptr, (size_t)rowsizes[i]);
              srcptr += rowsizes[i];
              dstptr += layouts[i].Footprint.RowPitch;
            }

            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.pResource = uploadBuf;
            src.PlacedFootprint = layouts[i];

            // copy buffer into this array slice
            cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

            // this slice now needs to be in shader-read to copy to the MSAA texture
            D3D12_RESOURCE_BARRIER b = {};
            b.Transition.pResource = tex;
            b.Transition.Subresource = i;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cmd->ResourceBarrier(1, &b);
          }

          D3D12_RESOURCE_BARRIER b = {};
          b.Transition.pResource = cb;
          b.Transition.Subresource = 0;
          b.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
          b.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
          cmd->ResourceBarrier(1, &b);

          cmd->Close();

          D3D12_RANGE range = {0, (SIZE_T)totalbytes};
          uploadBuf->Unmap(0, &range);

          Submit({cmd});
          GPUSync();
        }

        D3D12_GPU_DESCRIPTOR_HANDLE view =
            MakeSRV(tex).Format(viewFmt).PlaneSlice(0).CreateGPU(texidx * 2 + 0);

        // don't need to keep this handle, it's in the same 'table' as above
        if(view2Fmt != DXGI_FORMAT_UNKNOWN)
        {
          MakeSRV(tex).Format(view2Fmt).PlaneSlice(1).CreateGPU(texidx * 2 + 1);
        }
        else
        {
          // Create dummy descriptor
          D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
          cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) *
                     (texidx * 2 + 1);

          D3D12_SHADER_RESOURCE_VIEW_DESC dummydesc = {};
          dummydesc.Format = viewFmt;
          dummydesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
          dummydesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
          dummydesc.Texture2D.MipLevels = 1;

          dev->CreateShaderResourceView(NULL, &dummydesc, cpu);
        }

        textures[texidx] = {tex, name, view, cb};
      }
      texidx++;
    };

#define MAKE_TEX(sampling, texFmt, viewFmt, config, data_vector) \
  make_tex(#texFmt, sampling, texFmt, viewFmt, DXGI_FORMAT_UNKNOWN, config, data_vector.data());
#define MAKE_TEX2(sampling, texFmt, viewFmt, view2Fmt, config, data_vector) \
  make_tex(#texFmt, sampling, texFmt, viewFmt, view2Fmt, config, data_vector.data());

    MAKE_TEX(444, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4i(0, 0, 0, 0),
             rgba8.data);

    TEST_ASSERT(textures[0].views.ptr, "Expect RGBA8 to always work");

    MAKE_TEX(444, DXGI_FORMAT_AYUV, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4i(2, 1, 0, 1), yuv8);
    MAKE_TEX(444, DXGI_FORMAT_Y416, DXGI_FORMAT_R16G16B16A16_UNORM, Vec4i(1, 0, 2, 1), yuv16);

    ///////////////////////////////////////
    // 4:4:4 10-bit, special case
    ///////////////////////////////////////

    {
      std::vector<uint32_t> y410;
      y410.reserve(rgba8.data.size());

      const uint16_t *in = yuv16.data();

      // pack down from 16-bit data
      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
      {
        const uint16_t U = in[0] >> 6;
        const uint16_t Y = in[1] >> 6;
        const uint16_t V = in[2] >> 6;
        const uint16_t A = in[3] >> 14;
        in += 4;

        y410.push_back(uint32_t(A) << 30 | uint32_t(V) << 20 | uint32_t(Y) << 10 | uint32_t(U));
      }

      MAKE_TEX(444, DXGI_FORMAT_Y410, DXGI_FORMAT_R10G10B10A2_UNORM, Vec4i(1, 0, 2, 1), y410);
    }

    ///////////////////////////////////////
    // 4:2:2
    ///////////////////////////////////////
    {
      std::vector<byte> yuy2;
      yuy2.reserve(rgba8.data.size());

      const byte *in = yuv8.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i += 2)
      {
        // y0
        yuy2.push_back(in[2 + 0]);
        // avg(u0, u1)
        yuy2.push_back(byte((uint16_t(in[1 + 0]) + uint16_t(in[1 + 4])) >> 1));
        // y1
        yuy2.push_back(in[2 + 4]);
        // avg(v0, v1)
        yuy2.push_back(byte((uint16_t(in[0 + 0]) + uint16_t(in[0 + 4])) >> 1));

        in += 8;
      }

      MAKE_TEX(422, DXGI_FORMAT_YUY2, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4i(0, 1, 3, 1), yuy2);
    }

    {
      std::vector<byte> p208;
      p208.reserve(rgba8.data.size());

      const byte *in = yuv8.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
      {
        p208.push_back(in[1]);
        in += 4;
      }

      in = yuv8.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i += 2)
      {
        // avg(u0, u1)
        p208.push_back(byte((uint16_t(in[2 + 0]) + uint16_t(in[2 + 4])) >> 1));
        // avg(v0, v1)
        p208.push_back(byte((uint16_t(in[0 + 0]) + uint16_t(in[0 + 4])) >> 1));
        in += 8;
      }

      MAKE_TEX2(422, DXGI_FORMAT_P208, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM,
                Vec4i(0, 4, 5, 1), p208);
    }

    {
      std::vector<uint16_t> y216;
      y216.reserve(yuv16.size());

      const uint16_t *in = yuv16.data();

      for(uint32_t i = 0; i < rgba8.width * rgba8.height; i += 2)
      {
        // y0
        y216.push_back(in[1 + 0]);
        // avg(u0, u1)
        y216.push_back(uint16_t((uint32_t(in[0 + 0]) + uint32_t(in[0 + 4])) >> 1));
        // y1
        y216.push_back(in[1 + 4]);
        // avg(v0, v1)
        y216.push_back(uint16_t((uint32_t(in[2 + 0]) + uint32_t(in[2 + 4])) >> 1));

        in += 8;
      }

      // we can re-use the same data for Y010 and Y016 as they share a format (with different bits)
      MAKE_TEX(422, DXGI_FORMAT_Y210, DXGI_FORMAT_R16G16B16A16_UNORM, Vec4i(0, 1, 3, 1), y216);
      MAKE_TEX(422, DXGI_FORMAT_Y216, DXGI_FORMAT_R16G16B16A16_UNORM, Vec4i(0, 1, 3, 1), y216);
    }

    {
      std::vector<byte> nv12;
      nv12.reserve(rgba8.data.size());

      {
        const byte *in = yuv8.data();

        // luma plane
        for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
        {
          const byte Y = in[2];
          in += 4;

          nv12.push_back(Y);
        }
      }

      for(uint32_t row = 0; row < rgba8.height - 1; row += 2)
      {
        const byte *in = yuv8.data() + rgba8.width * 4 * row;
        const byte *in2 = yuv8.data() + rgba8.width * 4 * (row + 1);

        for(uint32_t i = 0; i < rgba8.width; i += 2)
        {
          const uint16_t Ua = in[1 + 0];
          const uint16_t Ub = in[1 + 4];
          const uint16_t Uc = in2[1 + 0];
          const uint16_t Ud = in2[1 + 4];

          const uint16_t Va = in[0 + 0];
          const uint16_t Vb = in[0 + 4];
          const uint16_t Vc = in2[0 + 0];
          const uint16_t Vd = in2[0 + 4];

          // midpoint average sample
          uint16_t U = (Ua + Ub + Uc + Ud) >> 2;
          uint16_t V = (Va + Vb + Vc + Vd) >> 2;

          in += 8;
          in2 += 8;

          nv12.push_back(byte(U));
          nv12.push_back(byte(V));
        }
      }

      MAKE_TEX2(420, DXGI_FORMAT_NV12, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM,
                Vec4i(0, 4, 5, 1), nv12);
    }

    {
      std::vector<uint16_t> p016;
      p016.reserve(rgba8.data.size() * 2);

      {
        const uint16_t *in = yuv16.data();

        // luma plane
        for(uint32_t i = 0; i < rgba8.width * rgba8.height; i++)
        {
          const uint16_t Y = in[1];
          in += 4;

          p016.push_back(Y);
        }
      }

      for(uint32_t row = 0; row < rgba8.height - 1; row += 2)
      {
        const uint16_t *in = yuv16.data() + rgba8.width * 4 * row;
        const uint16_t *in2 = yuv16.data() + rgba8.width * 4 * (row + 1);

        for(uint32_t i = 0; i < rgba8.width; i += 2)
        {
          const uint32_t Ua = in[0 + 0];
          const uint32_t Ub = in[0 + 4];
          const uint32_t Uc = in2[0 + 0];
          const uint32_t Ud = in2[0 + 4];

          const uint32_t Va = in[2 + 0];
          const uint32_t Vb = in[2 + 4];
          const uint32_t Vc = in2[2 + 0];
          const uint32_t Vd = in2[2 + 4];

          // midpoint average sample
          uint32_t U = (Ua + Ub + Uc + Ud) / 4;
          uint32_t V = (Va + Vb + Vc + Vd) / 4;

          in += 8;
          in2 += 8;

          p016.push_back(uint16_t(U & 0xffff));
          p016.push_back(uint16_t(V & 0xffff));
        }
      }

      // we can re-use the same data for P010 and P016 as they share a format (with different bits)
      MAKE_TEX2(420, DXGI_FORMAT_P010, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM,
                Vec4i(0, 4, 5, 1), p016);
      MAKE_TEX2(420, DXGI_FORMAT_P016, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM,
                Vec4i(0, 4, 5, 1), p016);
    }

    ID3D12ResourcePtr vb = MakeBuffer().Data(verts);

    ID3D12RootSignaturePtr sig = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 2),
    });

    ID3D12PipelineStatePtr pso =
        MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).RTVs({DXGI_FORMAT_R8G8B8A8_UNORM});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      // don't do sRGB conversion, as we won't in the shader either
      D3D12_CPU_DESCRIPTOR_HANDLE rtv = MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM).CreateCPU(0);

      OMSetRenderTargets(cmd, {rtv}, {});

      ClearRenderTargetView(cmd, rtv, {0.4f, 0.5f, 0.6f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      float x = 1.0f, y = 1.0f;
      const float w = 48.0f, h = 48.0f;

      for(size_t i = 0; i < ARRAY_COUNT(textures); i++)
      {
        TextureData &tex = textures[i];

        if(tex.views.ptr)
        {
          cmd->SetMarker(1, tex.name, UINT(strlen(tex.name) + 1));

          cmd->SetGraphicsRootConstantBufferView(0, tex.cb->GetGPUVirtualAddress());
          cmd->SetGraphicsRootDescriptorTable(1, tex.views);

          RSSetViewport(cmd, {x, y, w, h, 0.0f, 1.0f});
          cmd->DrawInstanced(4, 1, 0, 0);
        }

        x += 50.0f;

        if(x + 1.0f >= (float)screenWidth)
        {
          x = 1.0f;
          y += 50.0f;
        }
      }

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
