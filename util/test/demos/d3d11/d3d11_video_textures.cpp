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

// forward declare a couple of interfaces trimmed out of our local copy of the MF headers
struct IMediaBuffer;
struct IPropertyStore;
struct INamedPropertyStore;

#include "dx/official/mfapi.h"
#include "dx/official/mfmediaengine.h"

COM_SMARTPTR(IMFDXGIDeviceManager);
COM_SMARTPTR(IMFMediaEngineClassFactory);
COM_SMARTPTR(IMFAttributes);
COM_SMARTPTR(IMFMediaEngine);
COM_SMARTPTR(IMFMediaEngineEx);
COM_SMARTPTR(IMFByteStream);

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

TEST(D3D11_Video_Textures, D3D11GraphicsTest), IMFMediaEngineNotify
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
    const wchar_t *name;
    ID3D11ShaderResourceViewPtr views[2];
    Vec4i config[2];
  };

  bool video_loaded = false;

  // implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return 1; }
  ULONG STDMETHODCALLTYPE Release() { return 1; }
  HRESULT STDMETHODCALLTYPE QueryInterface(const IID &iid, void **obj)
  {
    if(iid == __uuidof(IUnknown))
    {
      *obj = (IUnknown *)this;
      return S_OK;
    }
    else if(iid == __uuidof(IMFMediaEngineNotify))
    {
      *obj = (IMFMediaEngineNotify *)this;
      return S_OK;
    }

    return E_NOINTERFACE;
  }
  // implement IMFMediaEngineNotify
  HRESULT STDMETHODCALLTYPE EventNotify(DWORD ev, DWORD_PTR param1, DWORD param2)
  {
    if(ev == MF_MEDIA_ENGINE_EVENT_CANPLAY)
      video_loaded = true;
    else if(ev == MF_MEDIA_ENGINE_EVENT_ERROR)
      TEST_ERROR("Error loading video: %x", param2);
    return S_OK;
  }

  int main()
  {
    // check for the existence of the test video
    std::string video_filename = GetDataPath("h264_yu420p_192x108_24fps.mp4");

    FILE *f = fopen(video_filename.c_str(), "rb");
    if(f)
      fclose(f);
    else
      video_filename = "";

    using PFN_MFCreateDXGIDeviceManager = decltype(&MFCreateDXGIDeviceManager);
    using PFN_MFStartup = decltype(&MFStartup);
    using PFN_MFCreateFile = decltype(&MFCreateFile);
    using PFN_MFShutdown = decltype(&MFShutdown);
    using PFN_MFCreateAttributes = decltype(&MFCreateAttributes);

    PFN_MFCreateDXGIDeviceManager dyn_MFCreateDXGIDeviceManager = NULL;
    PFN_MFStartup dyn_MFStartup = NULL;
    PFN_MFCreateFile dyn_MFCreateFile = NULL;
    PFN_MFShutdown dyn_MFShutdown = NULL;
    PFN_MFCreateAttributes dyn_MFCreateAttributes = NULL;

    HMODULE mfplat = LoadLibraryA("mfplat.dll");

    if(mfplat && !video_filename.empty())
    {
      dyn_MFCreateDXGIDeviceManager =
          (PFN_MFCreateDXGIDeviceManager)GetProcAddress(mfplat, "MFCreateDXGIDeviceManager");
      dyn_MFStartup = (PFN_MFStartup)GetProcAddress(mfplat, "MFStartup");
      dyn_MFShutdown = (PFN_MFShutdown)GetProcAddress(mfplat, "MFShutdown");
      dyn_MFCreateFile = (PFN_MFCreateFile)GetProcAddress(mfplat, "MFCreateFile");
      dyn_MFCreateAttributes = (PFN_MFCreateAttributes)GetProcAddress(mfplat, "MFCreateAttributes");

      if(dyn_MFCreateDXGIDeviceManager && dyn_MFStartup && dyn_MFCreateFile && dyn_MFCreateAttributes)
      {
        createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        dyn_MFStartup(MF_VERSION, MFSTARTUP_FULL);

        TEST_LOG("Initialising MediaFoundation");
      }
      else
      {
        dyn_MFCreateDXGIDeviceManager = NULL;
        dyn_MFStartup = NULL;
        dyn_MFShutdown = NULL;
        dyn_MFCreateFile = NULL;
        dyn_MFCreateAttributes = NULL;

        TEST_LOG("MediaFoundation not available");
      }
    }

    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    IMFMediaEnginePtr engine = NULL;

    // if we initialised MF this create flag will be set
    if(createFlags & D3D11_CREATE_DEVICE_VIDEO_SUPPORT)
    {
      // need to enable multithreaded as MediaFoundation breaks threading rules if any rendering is
      // going on
      {
        ID3D11MultithreadPtr mt;
        dev->QueryInterface(&mt);
        mt->SetMultithreadProtected(true);
      }

      IMFDXGIDeviceManagerPtr dxgiManager;

      // create DXGI Manager
      {
        UINT resetToken = 0;
        CHECK_HR(dyn_MFCreateDXGIDeviceManager(&resetToken, &dxgiManager));

        CHECK_HR(dxgiManager->ResetDevice(dev, resetToken));
      }

      // create class factory
      IMFMediaEngineClassFactoryPtr classFactory;
      CHECK_HR(CoCreateInstance(CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IMFMediaEngineClassFactory), (void **)&classFactory));

      // initialise attributes where we'll store our init properties
      IMFAttributesPtr attr;
      dyn_MFCreateAttributes(&attr, 3);

      CHECK_HR(attr->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, dxgiManager));
      CHECK_HR(attr->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_NV12));
      CHECK_HR(attr->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, this));

      // create the media engine itself
      CHECK_HR(classFactory->CreateInstance(0, attr, &engine));

      // set it looping
      CHECK_HR(engine->SetLoop(true));

      const std::wstring filename = UTF82Wide(video_filename);

      // open a bytestream for the file
      IMFByteStreamPtr byteStream;
      CHECK_HR(dyn_MFCreateFile(MF_ACCESSMODE_READ, MF_OPENMODE_FAIL_IF_NOT_EXIST,
                                MF_FILEFLAGS_NONE, filename.c_str(), &byteStream));

      size_t len = filename.length();

      // make a BSTR with the URL
      wchar_t *url = new wchar_t[len + 1 + 8];

      memcpy(url + 1 + 8, filename.c_str(), len * sizeof(wchar_t));
      memcpy(url + 1, L"file:///", 8 * sizeof(wchar_t));
      url[0] = (wchar_t)(len + 8);

      for(size_t i = 1; i < len + 1 + 8; i++)
        if(url[i] == '\\')
          url[i] = '/';

      // query for IMFMediaEngineEx so we can set the source from a byte stream
      {
        IMFMediaEngineExPtr engineex;
        CHECK_HR(engine->QueryInterface(&engineex));

        CHECK_HR(engineex->SetSourceFromByteStream(byteStream, url));
      }

      delete[] url;

      // wait for the video to load
      for(int i = 0; i < 300; i++)
      {
        if(video_loaded)
          break;
        Sleep(10);
      }

      if(!video_loaded)
        TEST_FATAL("Video wasn't playable after 3 seconds");
    }

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

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

    UINT reqsupp = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_LOAD;

    TextureData textures[20] = {};
    size_t texidx = 0;

    auto make_tex = [&](const wchar_t *name, uint32_t subsampling, DXGI_FORMAT texFmt,
                        DXGI_FORMAT viewFmt, DXGI_FORMAT view2Fmt, Vec4i config, void *data,
                        UINT rowPitch) {
      UINT supp = 0;
      dev->CheckFormatSupport(texFmt, &supp);

      {
        TEST_LOG("%ls supports:", name);
        if(supp == 0)
          TEST_LOG("  - NONE");
#define CHECK_SUPP(s)                 \
  if(supp & D3D11_FORMAT_SUPPORT_##s) \
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
        CHECK_SUPP(MIP_AUTOGEN)
        CHECK_SUPP(RENDER_TARGET)
        CHECK_SUPP(BLENDABLE)
        CHECK_SUPP(DEPTH_STENCIL)
        CHECK_SUPP(CPU_LOCKABLE)
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

      if((supp & reqsupp) == reqsupp)
      {
        ID3D11Texture2DPtr tex = MakeTexture(texFmt, rgba8.width, rgba8.height).Mips(1).SRV();

        // discard the resource when possible, this makes renderdoc treat it as dirty
        if(ctx1)
          ctx1->DiscardResource(tex);

        ctx->UpdateSubresource(tex, 0, NULL, data, rowPitch, 0);

        ID3D11ShaderResourceViewPtr view = MakeSRV(tex).Format(viewFmt);
        ID3D11ShaderResourceViewPtr view2;

        if(view2Fmt != DXGI_FORMAT_UNKNOWN)
          view2 = MakeSRV(tex).Format(view2Fmt);

        textures[texidx] = {
            name,
            {view, view2},
            {Vec4i(rgba8.width, rgba8.height, horizDownsampleFactor, vertDownsampleFactor), config},
        };
      }
      texidx++;
    };

#define MAKE_TEX(sampling, texFmt, viewFmt, config, data_vector, stride)                         \
  make_tex(L#texFmt, sampling, texFmt, viewFmt, DXGI_FORMAT_UNKNOWN, config, data_vector.data(), \
           stride);
#define MAKE_TEX2(sampling, texFmt, viewFmt, view2Fmt, config, data_vector, stride) \
  make_tex(L#texFmt, sampling, texFmt, viewFmt, view2Fmt, config, data_vector.data(), stride);

    MAKE_TEX(444, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4i(0, 0, 0, 0),
             rgba8.data, rgba8.width * 4);

    TEST_ASSERT(textures[0].views[0], "Expect RGBA8 to always work");

    MAKE_TEX(444, DXGI_FORMAT_AYUV, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4i(2, 1, 0, 1), yuv8,
             rgba8.width * 4);
    MAKE_TEX(444, DXGI_FORMAT_Y416, DXGI_FORMAT_R16G16B16A16_UNORM, Vec4i(1, 0, 2, 1), yuv16,
             rgba8.width * 8);

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

      MAKE_TEX(444, DXGI_FORMAT_Y410, DXGI_FORMAT_R10G10B10A2_UNORM, Vec4i(1, 0, 2, 1), y410,
               rgba8.width * 4);
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

      MAKE_TEX(422, DXGI_FORMAT_YUY2, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4i(0, 1, 3, 2), yuy2,
               rgba8.width * 2);
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
                Vec4i(0, 4, 5, 1), p208, rgba8.width);
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
      MAKE_TEX(422, DXGI_FORMAT_Y210, DXGI_FORMAT_R16G16B16A16_UNORM, Vec4i(0, 1, 3, 2), y216,
               rgba8.width * 4);
      MAKE_TEX(422, DXGI_FORMAT_Y216, DXGI_FORMAT_R16G16B16A16_UNORM, Vec4i(0, 1, 3, 2), y216,
               rgba8.width * 4);
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
                Vec4i(0, 4, 5, 1), nv12, rgba8.width);
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
                Vec4i(0, 4, 5, 1), p016, rgba8.width * 2);
      MAKE_TEX2(420, DXGI_FORMAT_P016, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM,
                Vec4i(0, 4, 5, 1), p016, rgba8.width * 2);
    }

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(verts);
    ID3D11BufferPtr cb = MakeBuffer().Constant().Size(sizeof(Vec4i) * 2);

    // don't do sRGB conversion, as we won't in the shader either
    ID3D11RenderTargetViewPtr bbDirectRTV = MakeRTV(bbTex).Format(DXGI_FORMAT_R8G8B8A8_UNORM);

    IDXGISurfacePtr videoSurface = NULL;
    ID3D11ShaderResourceViewPtr videoSRVs[2] = {};

    // if we got a media engine, create a surface to render to
    if(engine)
    {
      DWORD videoWidth = 0, videoHeight = 0;
      CHECK_HR(engine->GetNativeVideoSize(&videoWidth, &videoHeight));

      if(videoWidth > 0 && videoHeight > 0)
      {
        ID3D11Texture2DPtr tex =
            MakeTexture(DXGI_FORMAT_NV12, videoWidth, videoHeight).Mips(1).SRV().RTV();
        tex->QueryInterface(&videoSurface);
        videoSRVs[0] = MakeSRV(tex).Format(DXGI_FORMAT_R8_UNORM);
        videoSRVs[1] = MakeSRV(tex).Format(DXGI_FORMAT_R8G8_UNORM);
      }

      // start playing the video
      CHECK_HR(engine->Play());
    }

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);
      ctx->PSSetConstantBuffers(0, 1, &cb.GetInterfacePtr());

      ctx->OMSetRenderTargets(1, &bbDirectRTV.GetInterfacePtr(), NULL);

      float x = 1.0f, y = 1.0f;
      const float w = 48.0f, h = 48.0f;

      for(size_t i = 0; i < ARRAY_COUNT(textures); i++)
      {
        TextureData &tex = textures[i];

        if(tex.views[0])
        {
          if(annot)
            annot->SetMarker(tex.name);

          ctx->UpdateSubresource(cb, 0, NULL, tex.config, sizeof(tex.config), sizeof(tex.config));

          RSSetViewport({x, y, w, h, 0.0f, 1.0f});
          ctx->PSSetShaderResources(0, 2, (ID3D11ShaderResourceView **)tex.views);
          ctx->Draw(4, 0);
        }

        x += 50.0f;

        if(x + 1.0f >= (float)screenWidth)
        {
          x = 1.0f;
          y += 50.0f;
        }
      }

      if(engine && videoSurface)
      {
        if(annot)
          annot->BeginEvent(L"Video");

        DWORD videoWidth = 0, videoHeight = 0;
        CHECK_HR(engine->GetNativeVideoSize(&videoWidth, &videoHeight));

        LONGLONG timestamp = 0;
        if(engine->OnVideoStreamTick(&timestamp) == S_OK)
        {
          if(annot)
            annot->SetMarker(L"Video Surface Update");

          MFVideoNormalizedRect srcRect = {0.0f, 0.0f, 1.0f, 1.0f};
          RECT dstRect = {0, 0, (LONG)videoWidth, (LONG)videoHeight};
          MFARGB fillColor = {};

          engine->TransferVideoFrame(videoSurface, &srcRect, &dstRect, &fillColor);
        }

        RSSetViewport({0.0f, 100.0f, 356.0f, 200.0f, 0.0f, 1.0f});

        Vec4i videoConfig[] = {
            Vec4i(videoWidth, videoHeight, 2, 2), Vec4i(0, 4, 5, 1),
        };

        if(annot)
          annot->SetMarker(L"Video Surface Blit");

        ctx->UpdateSubresource(cb, 0, NULL, videoConfig, sizeof(videoConfig), sizeof(videoConfig));

        ctx->PSSetShaderResources(0, 2, (ID3D11ShaderResourceView **)videoSRVs);
        ctx->Draw(4, 0);

        if(annot)
          annot->EndEvent();
      }

      Present();
    }

    engine = NULL;

    if(dyn_MFShutdown)
      dyn_MFShutdown();

    return 0;
  }
};

REGISTER_TEST();
