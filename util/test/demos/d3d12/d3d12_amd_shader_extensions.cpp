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

#include "../win32/win32_window.h"

#include "3rdparty/ags/ags_shader_intrinsics_dx12.hlsl.h"
#include "3rdparty/ags/amd_ags.h"

RD_TEST(D3D12_AMD_Shader_Extensions, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests using AMD shader extensions on D3D12.";

  AGS_INITIALIZE dyn_agsInitialize = NULL;
  AGS_DEINITIALIZE dyn_agsDeInitialize = NULL;

  AGS_DRIVEREXTENSIONSDX12_CREATEDEVICE dyn_agsDriverExtensionsDX12_CreateDevice = NULL;
  AGS_DRIVEREXTENSIONSDX12_DESTROYDEVICE dyn_agsDriverExtensionsDX12_DestroyDevice = NULL;

  AGSContext *ags = NULL;

  std::string BaryCentricPixel = R"EOSHADER(

float4 main() : SV_Target0
{
  float2 bary = AmdExtD3DShaderIntrinsics_IjBarycentricCoords( AmdExtD3DShaderIntrinsicsBarycentric_LinearCenter );
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

RWByteAddressBuffer inUAV : register(u1);
RWByteAddressBuffer outUAV : register(u2);

[numthreads(256, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    // read input from source
    uint2 input;
    input.x = inUAV.Load(threadID.x * 8);
    input.y = inUAV.Load(threadID.x * 8 + 4);
    
    AmdExtD3DShaderIntrinsics_AtomicMaxU64(outUAV, 0, input);
}

)EOSHADER";

  std::string BasicPixel = R"EOSHADER(

float4 main() : SV_Target0
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f);
}

)EOSHADER";

  std::string BasicCompute = R"EOSHADER(

RWByteAddressBuffer inUAV : register(u1);
RWByteAddressBuffer outUAV : register(u2);

[numthreads(256, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    // read input from source
    uint2 input;
    input.x = inUAV.Load(threadID.x * 8);
    input.y = inUAV.Load(threadID.x * 8 + 4);
    
    outUAV.Store(0, input);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(m_12On7)
    {
      Avail = "Can't test AGS DX12 on 12On7";
      return;
    }

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

    dyn_agsDriverExtensionsDX12_CreateDevice = (AGS_DRIVEREXTENSIONSDX12_CREATEDEVICE)GetProcAddress(
        agsLib, "agsDriverExtensionsDX12_CreateDevice");
    dyn_agsDriverExtensionsDX12_DestroyDevice =
        (AGS_DRIVEREXTENSIONSDX12_DESTROYDEVICE)GetProcAddress(
            agsLib, "agsDriverExtensionsDX12_DestroyDevice");

    if(!dyn_agsInitialize || !dyn_agsDeInitialize || !dyn_agsDriverExtensionsDX12_CreateDevice ||
       !dyn_agsDriverExtensionsDX12_DestroyDevice)
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

    ID3D12Device *ags_devHandle = NULL;
    CreateExtendedDevice(&ags_devHandle, NULL);

    // once we've checked that we can create an extension device on an adapter, we can release it
    // and return ready to run
    if(ags_devHandle)
    {
      Avail = "";
      unsigned int dummy = 0;
      dyn_agsDriverExtensionsDX12_DestroyDevice(ags, ags_devHandle, &dummy);
      return;
    }

    Avail = "AGS couldn't create device on any selected adapter.";
  }

  void CreateExtendedDevice(ID3D12Device * *ags_devHandle,
                            AGSDX12ReturnedParams::ExtensionsSupported * exts)
  {
    std::vector<IDXGIAdapterPtr> adapters = GetAdapters();

    for(IDXGIAdapterPtr a : adapters)
    {
      AGSDX12DeviceCreationParams devCreate = {};
      AGSDX12ExtensionParams extCreate = {};
      AGSDX12ReturnedParams ret = {};

      devCreate.FeatureLevel = minFeatureLevel;
      devCreate.iid = __uuidof(ID3D12Device);
      devCreate.pAdapter = a.GetInterfacePtr();

      extCreate.pAppName = L"RenderDoc demos";
      extCreate.pEngineName = L"RenderDoc demos";

      AGSReturnCode agsret =
          dyn_agsDriverExtensionsDX12_CreateDevice(ags, &devCreate, &extCreate, &ret);

      if(agsret == AGS_SUCCESS && ret.pDevice)
      {
        // don't accept devices that don't support the intrinsics we want
        if(!ret.extensionsSupported.intrinsics16 || !ret.extensionsSupported.intrinsics19)
        {
          unsigned int dummy = 0;
          dyn_agsDriverExtensionsDX12_DestroyDevice(ags, ret.pDevice, &dummy);
        }
        else
        {
          *ags_devHandle = ret.pDevice;
          if(exts)
            *exts = ret.extensionsSupported;
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

    d3d12Debug = NULL;

    queue = NULL;

    m_GPUSyncFence = NULL;

    m_Alloc = NULL;
    m_DebugList = NULL;

    m_RTV = NULL;
    m_DSV = NULL;
    m_Sampler = NULL;
    m_CBVUAVSRV = NULL;

    m_Clear = NULL;

    m_ReadbackBuffer = NULL;
    m_UploadBuffer = NULL;

    swapBlitSig = NULL;
    swapBlitPso = NULL;

    infoqueue = NULL;

    dev1 = NULL;
    dev2 = NULL;
    dev3 = NULL;
    dev4 = NULL;
    dev5 = NULL;
    dev6 = NULL;
    dev7 = NULL;
    dev8 = NULL;

    // and swapchain & related
    swap = NULL;
    bbTex[0] = bbTex[1] = NULL;

    // we don't use these directly, just copy them into the real device so we know that ags is the
    // one to destroy the last reference to the device
    ID3D12Device *ags_devHandle = NULL;
    AGSDX12ReturnedParams::ExtensionsSupported features = {};
    CreateExtendedDevice(&ags_devHandle, &features);

    dev = ags_devHandle;

    if(!features.UAVBindSlot || !features.intrinsics16 || !features.intrinsics19)
    {
      dev = NULL;
      TEST_ERROR("Couldn't create AMD device with required features");
      return 4;
    }

    // recreate things we need on the new device
    PostDeviceCreate();

    // create the swapchain on the new AGS-extended device
    DXGI_SWAP_CHAIN_DESC1 swapDesc = MakeSwapchainDesc();

    {
      IDXGIFactory4Ptr factory4 = m_Factory;

      CHECK_HR(factory4->CreateSwapChainForHwnd(queue, ((Win32Window *)mainWindow)->wnd, &swapDesc,
                                                NULL, NULL, &swap));

      CHECK_HR(swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&bbTex[0]));
      CHECK_HR(swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&bbTex[1]));
    }

    ID3D12RootSignaturePtr sig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 3, 0),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                   AGS_DX12_SHADER_INSTRINSICS_SPACE_ID, 0, 1, 3),
    });

    std::string ags_header = ags_shader_intrinsics_dx12_hlsl();

    const std::string profilesuffix[2] = {"_5_1", "_6_0"};
    const std::wstring namesuffix[2] = {L"SM51", L"SM60"};

    ID3DBlobPtr vsblob[2];
    ID3DBlobPtr psblob[2];
    ID3DBlobPtr csblob[2];

    bool valid[2] = {};
    ID3D12PipelineStatePtr pso[2];
    ID3D12PipelineStatePtr cso[2];

    for(int i = 0; i < ARRAY_COUNT(profilesuffix); i++)
    {
      vsblob[i] = Compile(D3DDefaultVertex, "main", "vs" + profilesuffix[i]);

      // if we don't have DXIL support we can't compile anything, even a dummy shader
      if(i == 2 && !m_DXILSupport)
        continue;

      valid[i] = true;

      // can't skip optimising and still have the extensions work, sadly
      psblob[i] = Compile(ags_header + BaryCentricPixel, "main", "ps" + profilesuffix[i], false);
      csblob[i] = Compile(ags_header + MaxCompute, "main", "cs" + profilesuffix[i], false);

      pso[i] = MakePSO().RootSig(sig).InputLayout().VS(vsblob[i]).PS(psblob[i]);
      cso[i] = MakePSO().RootSig(sig).CS(csblob[i]);

      pso[i]->SetName((L"pipe" + namesuffix[i]).c_str());
      cso[i]->SetName((L"cspipe" + namesuffix[i]).c_str());
    }

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

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

    ID3D12ResourcePtr inBuf =
        MakeBuffer().UAV().Data(values.data()).Size(UINT(sizeof(uint64_t) * values.size()));
    ID3D12ResourcePtr outBuf = MakeBuffer().UAV().Size(32);

    ResourceBarrier(inBuf, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ResourceBarrier(outBuf, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    outBuf->SetName(L"outBuf");

    D3D12_GPU_DESCRIPTOR_HANDLE uav0gpu =
        MakeUAV(inBuf).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().CreateGPU(1);
    D3D12_GPU_DESCRIPTOR_HANDLE uav1gpu =
        MakeUAV(outBuf).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().CreateGPU(2);

    D3D12_CPU_DESCRIPTOR_HANDLE uav0cpu =
        MakeUAV(inBuf).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().CreateClearCPU(1);
    D3D12_CPU_DESCRIPTOR_HANDLE uav1cpu =
        MakeUAV(outBuf).Format(DXGI_FORMAT_R32_TYPELESS).ByteAddressed().CreateClearCPU(2);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      // force inclusion of all pipelines
      for(int i = 0; i < ARRAY_COUNT(pso); i++)
      {
        if(pso[i])
          cmd->SetPipelineState(pso[i]);
        if(cso[i])
          cmd->SetPipelineState(cso[i]);
      }

      cmd->Close();

      Submit({cmd});

      float x = (float)screenWidth / 2.0f;
      float y = (float)screenHeight / 2.0f;

      const std::string passname[] = {"SM50", "SM51", "SM60"};
      for(int i = 0; i < ARRAY_COUNT(pso); i++)
      {
        if(!valid[i])
          continue;

        cmd = GetCommandBuffer();

        Reset(cmd);

        pushMarker(cmd, passname[i]);

        UINT zero[4] = {};
        cmd->ClearUnorderedAccessViewUint(uav1gpu, uav1cpu, outBuf, zero, 0, NULL);

        OMSetRenderTargets(cmd, {rtv}, {});

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        RSSetViewport(cmd, {x * float(i % 2), y * float(i / 2), x, y, 0.0f, 1.0f});

        setMarker(cmd, passname[i] + " Draw");
        cmd->SetPipelineState(pso[i]);
        cmd->SetGraphicsRootSignature(sig);

        cmd->DrawInstanced(3, 1, 0, 0);

        cmd->SetPipelineState(cso[i]);
        cmd->SetComputeRootSignature(sig);
        cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

        setMarker(cmd, passname[i] + " Dispatch");

        cmd->Dispatch(numInputValues / 256, 1, 1);

        cmd->Close();

        Submit({cmd});

        GPUSync();

        std::vector<byte> output = GetBufferData(outBuf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 8);

        cmd = GetCommandBuffer();

        Reset(cmd);

        uint64_t gpuMax = 0;
        memcpy(&gpuMax, output.data(), sizeof(uint64_t));

        setMarker(cmd, passname[i] + " cpuMax: " + std::to_string(cpuMax));
        setMarker(cmd, passname[i] + " gpuMax: " + std::to_string(gpuMax));

        popMarker(cmd);

        cmd->Close();

        Submit({cmd});
      }

      cmd = GetCommandBuffer();

      Reset(cmd);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    dev = NULL;

    unsigned int dummy = 0;
    dyn_agsDriverExtensionsDX12_DestroyDevice(ags, ags_devHandle, &dummy);

    dyn_agsDeInitialize(ags);

    return 0;
  }
};

REGISTER_TEST();
