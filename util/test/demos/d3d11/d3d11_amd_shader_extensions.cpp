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

#include "3rdparty/ags/ags_shader_intrinsics_dx11.hlsl.h"
#include "3rdparty/ags/amd_ags.h"

RD_TEST(D3D11_AMD_Shader_Extensions, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests using AMD shader extensions on D3D11.";

  AGS_INITIALIZE dyn_agsInitialize = NULL;
  AGS_DEINITIALIZE dyn_agsDeInitialize = NULL;

  AGS_DRIVEREXTENSIONSDX11_CREATEDEVICE dyn_agsDriverExtensionsDX11_CreateDevice = NULL;
  AGS_DRIVEREXTENSIONSDX11_DESTROYDEVICE dyn_agsDriverExtensionsDX11_DestroyDevice = NULL;

  AGSContext *ags = NULL;

  std::string BaryCentricPixel = R"EOSHADER(

float4 main() : SV_Target0
{
  float2 bary = AmdDxExtShaderIntrinsics_IjBarycentricCoords( AmdDxExtShaderIntrinsicsBarycentric_LinearCenter );
  float3 bary3 = float3(bary.x, bary.y, 1.0 - (bary.x + bary.y));

  if(bary3.x > bary3.y && bary3.x > bary3.z)
     return float4(1.0f, 0.0f, 0.0f, 1.0f);
  else if(bary3.y > bary3.x && bary3.y > bary3.z)
     return float4(0.0f, 1.0f, 0.0f, 1.0f);
  else
     return float4(0.0f, 0.0f, 1.0f, 1.0f);
}

)EOSHADER";

  std::string MaxCompute = R"EOSHADER(

RWByteAddressBuffer inUAV : register(u0);
RWByteAddressBuffer outUAV : register(u1);

[numthreads(256, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    // read input from source
    uint2 input;
    input.x = inUAV.Load(threadID.x * 8);
    input.y = inUAV.Load(threadID.x * 8 + 4);
    
    AmdDxExtShaderIntrinsics_AtomicMaxU64(outUAV, 0, input);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    D3D11GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    std::string agsname = sizeof(void *) == 8 ? "amd_ags_x64.dll" : "amd_ags_x86.dll";

    HMODULE agsLib = LoadLibraryA(agsname.c_str());

    // try in local plugins folder
    if(!agsLib)
    {
      agsLib = LoadLibraryA(
          ("../../" + std::string(sizeof(void *) == 8 ? "plugins-win64/" : "plugins/win32/") +
           "amd/ags/" + agsname)
              .c_str());
    }

    if(!agsLib)
    {
      // try in plugins folder next to renderdoc.dll
      HMODULE rdocmod = GetModuleHandleA("renderdoc.dll");
      char path[MAX_PATH + 1] = {};

      if(rdocmod)
      {
        GetModuleFileNameA(rdocmod, path, MAX_PATH);
        std::string tmp = path;
        tmp.resize(tmp.size() - (sizeof("/renderdoc.dll") - 1));

        agsLib = LoadLibraryA((tmp + "/plugins/amd/ags/" + agsname).c_str());
      }
    }

    if(!agsLib)
    {
      Avail = "Couldn't load AGS dll";
      return;
    }

    dyn_agsInitialize = (AGS_INITIALIZE)GetProcAddress(agsLib, "agsInitialize");
    dyn_agsDeInitialize = (AGS_DEINITIALIZE)GetProcAddress(agsLib, "agsDeInitialize");

    dyn_agsDriverExtensionsDX11_CreateDevice = (AGS_DRIVEREXTENSIONSDX11_CREATEDEVICE)GetProcAddress(
        agsLib, "agsDriverExtensionsDX11_CreateDevice");
    dyn_agsDriverExtensionsDX11_DestroyDevice =
        (AGS_DRIVEREXTENSIONSDX11_DESTROYDEVICE)GetProcAddress(
            agsLib, "agsDriverExtensionsDX11_DestroyDevice");

    if(!dyn_agsInitialize || !dyn_agsDeInitialize || !dyn_agsDriverExtensionsDX11_CreateDevice ||
       !dyn_agsDriverExtensionsDX11_DestroyDevice)
    {
      Avail = "AGS didn't have all necessary entry points - too old?";
      return;
    }

    AGSReturnCode agsret = dyn_agsInitialize(
        AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH), NULL,
        &ags, NULL);

    if(agsret != AGS_SUCCESS || ags == NULL)
    {
      Avail = "AGS couldn't initialise";
      return;
    }

    ID3D11Device *ags_devHandle = NULL;
    ID3D11DeviceContext *ags_ctxHandle = NULL;
    CreateExtendedDevice(&ags_devHandle, &ags_ctxHandle);

    // once we've checked that we can create an extension device on an adapter, we can release it
    // and return ready to run
    if(ags_devHandle)
    {
      Avail = "";
      unsigned int dummy = 0;
      dyn_agsDriverExtensionsDX11_DestroyDevice(ags, ags_devHandle, &dummy, ags_ctxHandle, &dummy);
      return;
    }

    Avail = "AGS couldn't create device on any selected adapter.";
  }

  void CreateExtendedDevice(ID3D11Device * *ags_devHandle, ID3D11DeviceContext * *ags_ctxHandle)
  {
    std::vector<IDXGIAdapterPtr> adapters = GetAdapters();

    for(IDXGIAdapterPtr a : adapters)
    {
      AGSDX11DeviceCreationParams devCreate = {};
      AGSDX11ExtensionParams extCreate = {};
      AGSDX11ReturnedParams ret = {};

      devCreate.pAdapter = a.GetInterfacePtr();
      devCreate.DriverType = D3D_DRIVER_TYPE_UNKNOWN;
      devCreate.Flags = createFlags | (debugDevice ? D3D11_CREATE_DEVICE_DEBUG : 0);
      devCreate.pFeatureLevels = &feature_level;
      devCreate.FeatureLevels = 1;
      devCreate.SDKVersion = D3D11_SDK_VERSION;

      extCreate.uavSlot = 7;
      extCreate.crossfireMode = AGS_CROSSFIRE_MODE_DISABLE;
      extCreate.pAppName = L"RenderDoc demos";
      extCreate.pEngineName = L"RenderDoc demos";

      AGSReturnCode agsret =
          dyn_agsDriverExtensionsDX11_CreateDevice(ags, &devCreate, &extCreate, &ret);

      if(agsret == AGS_SUCCESS && ret.pDevice)
      {
        // don't accept devices that don't support the intrinsics we want
        if(!ret.extensionsSupported.intrinsics16 || !ret.extensionsSupported.intrinsics19)
        {
          unsigned int dummy = 0;
          dyn_agsDriverExtensionsDX11_DestroyDevice(ags, ret.pDevice, &dummy, ret.pImmediateContext,
                                                    &dummy);
        }
        else
        {
          *ags_devHandle = ret.pDevice;
          *ags_ctxHandle = ret.pImmediateContext;
          return;
        }
      }
    }
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    // release the old device and swapchain
    dev = NULL;
    ctx = NULL;

    dev1 = NULL;
    dev2 = NULL;
    dev3 = NULL;
    dev4 = NULL;
    dev5 = NULL;

    ctx1 = NULL;
    ctx2 = NULL;
    ctx3 = NULL;
    ctx4 = NULL;

    annot = NULL;

    swapBlitVS = NULL;
    swapBlitPS = NULL;

    // and swapchain & related
    swap = NULL;
    bbTex = NULL;
    bbRTV = NULL;

    // we don't use these directly, just copy them into the real device so we know that ags is the
    // one to destroy the last reference to the device
    ID3D11Device *ags_devHandle = NULL;
    ID3D11DeviceContext *ags_ctxHandle = NULL;
    CreateExtendedDevice(&ags_devHandle, &ags_ctxHandle);

    dev = ags_devHandle;
    ctx = ags_ctxHandle;
    annot = ctx;

    // create the swapchain on the new AGS-extended device
    DXGI_SWAP_CHAIN_DESC swapDesc = MakeSwapchainDesc(mainWindow);

    HRESULT hr = fact->CreateSwapChain(dev, &swapDesc, &swap);
    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't create swapchain");
      return 4;
    }

    hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bbTex);
    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't get swapchain backbuffer");
      return 4;
    }

    hr = dev->CreateRenderTargetView(bbTex, NULL, &bbRTV);
    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't create swapchain RTV");
      return 4;
    }

    std::string ags_header = ags_shader_intrinsics_dx11_hlsl();

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    // can't skip optimising and still have the extensions work, sadly
    ID3DBlobPtr psblob = Compile(ags_header + BaryCentricPixel, "main", "ps_5_0", false);
    ID3DBlobPtr csblob = Compile(ags_header + MaxCompute, "main", "cs_5_0", false);

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);
    ID3D11ComputeShaderPtr cs = CreateCS(csblob);

    SetDebugName(cs, "cs");

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    // make a simple texture so that the structured data includes texture initial states
    ID3D11Texture2DPtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 4).RTV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    const int numInputValues = 16384;

    std::vector<uint64_t> values;
    values.resize(numInputValues);

    uint64_t cpuMax = 0;
    for(uint64_t &v : values)
    {
      v = 0;
      for(uint32_t byte = 0; byte < 8; byte++)
      {
        uint64_t b = (rand() & 0xff0) >> 4;
        v |= b << (byte * 8);
      }

      cpuMax = std::max(v, cpuMax);
    }

    ID3D11BufferPtr inBuf = MakeBuffer()
                                .UAV()
                                .ByteAddressed()
                                .Data(values.data())
                                .Size(UINT(sizeof(uint64_t) * values.size()));
    ID3D11BufferPtr outBuf = MakeBuffer().UAV().ByteAddressed().Size(32);

    SetDebugName(outBuf, "outBuf");

    ID3D11UnorderedAccessViewPtr inUAV = MakeUAV(inBuf).Format(DXGI_FORMAT_R32_TYPELESS);
    ID3D11UnorderedAccessViewPtr outUAV = MakeUAV(outBuf).Format(DXGI_FORMAT_R32_TYPELESS);

    while(Running())
    {
      ctx->ClearState();

      uint32_t zero[4] = {};
      ctx->ClearUnorderedAccessViewUint(outUAV, zero);

      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      ctx->Draw(3, 0);

      ctx->CSSetShader(cs, NULL, 0);

      ctx->CSSetUnorderedAccessViews(0, 1, &inUAV.GetInterfacePtr(), NULL);
      ctx->CSSetUnorderedAccessViews(1, 1, &outUAV.GetInterfacePtr(), NULL);

      ctx->Dispatch(numInputValues / 256, 1, 1);

      ctx->Flush();

      std::vector<byte> output = GetBufferData(outBuf, 0, 8);

      uint64_t gpuMax = 0;
      memcpy(&gpuMax, output.data(), sizeof(uint64_t));

      setMarker("cpuMax: " + std::to_string(cpuMax));
      setMarker("gpuMax: " + std::to_string(gpuMax));

      Present();
    }

    dev = NULL;
    ctx = NULL;
    annot = NULL;

    unsigned int dummy = 0;
    dyn_agsDriverExtensionsDX11_DestroyDevice(ags, ags_devHandle, &dummy, ags_ctxHandle, &dummy);

    dyn_agsDeInitialize(ags);

    return 0;
  }
};

REGISTER_TEST();
