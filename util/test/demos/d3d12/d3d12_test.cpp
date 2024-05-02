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

#define INITGUID

#include "d3d12_test.h"
#include <stdio.h>
#include "../3rdparty/lz4/lz4.h"
#include "../3rdparty/md5/md5.h"
#include "../renderdoc_app.h"
#include "../win32/win32_window.h"
#include "dx/official/dxcapi.h"

typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY1)(REFIID, void **);
typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY2)(UINT, REFIID, void **);

typedef DXC_API_IMPORT HRESULT(__stdcall *pDxcCreateInstance)(REFCLSID rclsid, REFIID riid,
                                                              LPVOID *ppv);

namespace
{
HMODULE d3d12 = NULL;
HMODULE dxgi = NULL;
HMODULE d3dcompiler = NULL;
HMODULE dxcompiler = NULL;
IDXGIFactory1Ptr factory;
std::vector<IDXGIAdapterPtr> adapters;
bool d3d12on7 = false;

pD3DCompile dyn_D3DCompile = NULL;
pD3DStripShader dyn_D3DStripShader = NULL;
pD3DSetBlobPart dyn_D3DSetBlobPart = NULL;
pD3DCreateBlob dyn_CreateBlob = NULL;

PFN_D3D12_CREATE_DEVICE dyn_D3D12CreateDevice = NULL;

PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE dyn_serializeRootSig;
PFN_D3D12_SERIALIZE_ROOT_SIGNATURE dyn_serializeRootSigOld;

struct DevicePointers
{
  ID3D12DebugPtr debug;
  ID3D12DeviceFactoryPtr factory;
  ID3D12DeviceConfigurationPtr config;
};

DevicePointers PrepareCreateDeviceFromDLL(const std::string &d3d12path, bool debug, bool gpuValidation)
{
  DevicePointers ret;

  HRESULT hr = S_OK;

  ID3D12DeviceFactoryPtr devfactory = NULL;
  if(!d3d12path.empty())
  {
#ifdef WIN64
#define BITNESS_SUFFIX "/x64"
#else
#define BITNESS_SUFFIX "/win32"
#endif

    PFN_D3D12_GET_INTERFACE getD3D12Interface =
        (PFN_D3D12_GET_INTERFACE)GetProcAddress(d3d12, "D3D12GetInterface");

    ID3D12SDKConfigurationPtr config = NULL;
    hr = getD3D12Interface(CLSID_D3D12SDKConfiguration, __uuidof(ID3D12SDKConfiguration),
                           (void **)&config);

    ID3D12SDKConfiguration1Ptr config1 = NULL;
    if(config)
      config1 = config;

    if(config1)
    {
      std::string path = d3d12path;

      // try to load d3d12core.dll - starting with if the argument is directly to the dll, otherwise
      // try increasing suffixes
      HMODULE mod = NULL;
      mod = LoadLibraryA(path.c_str());

      if(mod)
      {
        size_t trim = path.find_last_of("/\\");
        path[trim] = '\0';
      }
      else
      {
        mod = LoadLibraryA((path + "/d3d12core.dll").c_str());
        if(mod)
        {
          // path is the folder as needed
        }
        else
        {
          path = d3d12path + BITNESS_SUFFIX;
          mod = LoadLibraryA((path + "/d3d12core.dll").c_str());
          if(mod)
          {
          }
          else
          {
            path = d3d12path + "/bin" BITNESS_SUFFIX;
            mod = LoadLibraryA((path + "/d3d12core.dll").c_str());

            if(!mod)
            {
              TEST_LOG("Couldn't find D3D12 dll under path %s", d3d12path.c_str());
            }
          }
        }
      }

      if(mod)
      {
        DWORD *version = (DWORD *)GetProcAddress(mod, "D3D12SDKVersion");

        if(version)
        {
          hr = config1->CreateDeviceFactory(*version, path.c_str(), __uuidof(ID3D12DeviceFactory),
                                            (void **)&devfactory);

          if(FAILED(hr))
            devfactory = NULL;
        }
      }
    }

    if(!devfactory)
    {
      TEST_LOG("Tried to enable dynamic D3D12 SDK, but failed to get interface");
    }

    ret.factory = devfactory;
    ret.config = devfactory;
  }

  if(debug)
  {
    if(devfactory)
    {
      hr = devfactory->GetConfigurationInterface(CLSID_D3D12Debug, __uuidof(ID3D12Debug),
                                                 (void **)&ret.debug);

      if(FAILED(hr))
        ret.debug = NULL;
    }
    else
    {
      PFN_D3D12_GET_DEBUG_INTERFACE getD3D12DebugInterface =
          (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(d3d12, "D3D12GetDebugInterface");

      if(!getD3D12DebugInterface)
      {
        TEST_ERROR("Couldn't find D3D12GetDebugInterface!");
        return {};
      }

      hr = getD3D12DebugInterface(__uuidof(ID3D12Debug), (void **)&ret.debug);

      if(FAILED(hr))
        ret.debug = NULL;
    }

    if(ret.debug)
    {
      ret.debug->EnableDebugLayer();

      if(gpuValidation)
      {
        ID3D12Debug1Ptr debug1 = ret.debug;

        if(debug1)
          debug1->SetEnableGPUBasedValidation(true);
      }
    }
  }

  return ret;
}

};    // namespace

void D3D12GraphicsTest::Prepare(int argc, char **argv)
{
  GraphicsTest::Prepare(argc, argv);

  static bool prepared = false;

  if(!prepared)
  {
    prepared = true;

    d3d12 = LoadLibraryA("d3d12.dll");
    dxgi = LoadLibraryA("dxgi.dll");
    d3dcompiler = LoadLibraryA("d3dcompiler_47.dll");
    if(!d3dcompiler)
      d3dcompiler = LoadLibraryA("d3dcompiler_46.dll");
    if(!d3dcompiler)
      d3dcompiler = LoadLibraryA("d3dcompiler_45.dll");
    if(!d3dcompiler)
      d3dcompiler = LoadLibraryA("d3dcompiler_44.dll");
    if(!d3dcompiler)
      d3dcompiler = LoadLibraryA("d3dcompiler_43.dll");
    dxcompiler = LoadLibraryA("dxcompiler.dll");

    if(!d3d12)
    {
      d3d12 = LoadLibraryA("12on7/d3d12.dll");
      d3d12on7 = (d3d12 != NULL);
    }

    if(d3d12)
    {
      PFN_CREATE_DXGI_FACTORY1 createFactory1 =
          (PFN_CREATE_DXGI_FACTORY1)GetProcAddress(dxgi, "CreateDXGIFactory1");
      PFN_CREATE_DXGI_FACTORY2 createFactory2 =
          (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgi, "CreateDXGIFactory2");

      HRESULT hr = E_FAIL;

      if(createFactory2)
        hr = createFactory2(debugDevice ? DXGI_CREATE_FACTORY_DEBUG : 0, __uuidof(IDXGIFactory1),
                            (void **)&factory);
      else if(createFactory1)
        hr = createFactory1(__uuidof(IDXGIFactory1), (void **)&factory);

      if(SUCCEEDED(hr))
      {
        bool warp = false;

        adapters = FindD3DAdapters(factory, argc, argv, warp);

        if(warp && !d3d12on7)
        {
          IDXGIFactory4Ptr factory4 = factory;
          IDXGIAdapterPtr warpAdapter;
          if(factory4)
          {
            hr = factory4->EnumWarpAdapter(__uuidof(IDXGIAdapter), (void **)&warpAdapter);
            if(SUCCEEDED(hr))
              adapters.push_back(warpAdapter);
          }
        }
      }
    }

    if(d3dcompiler)
    {
      dyn_D3DCompile = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");
      dyn_D3DStripShader = (pD3DStripShader)GetProcAddress(d3dcompiler, "D3DStripShader");
      dyn_D3DSetBlobPart = (pD3DSetBlobPart)GetProcAddress(d3dcompiler, "D3DSetBlobPart");
      dyn_CreateBlob = (pD3DCreateBlob)GetProcAddress(d3dcompiler, "D3DCreateBlob");
    }

    if(d3d12)
    {
      dyn_D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice");

      dyn_serializeRootSig = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(
          d3d12, "D3D12SerializeVersionedRootSignature");
      dyn_serializeRootSigOld =
          (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(d3d12, "D3D12SerializeRootSignature");
    }
  }

  if(!d3d12)
    Avail = "d3d12.dll is not available";
  else if(!dxgi)
    Avail = "dxgi.dll is not available";
  else if(!d3dcompiler)
    Avail = "d3dcompiler_XX.dll is not available";
  else if(!factory)
    Avail = "Couldn't create DXGI factory";
  else if(!dyn_D3D12CreateDevice || !dyn_D3DCompile || !dyn_D3DStripShader || !dyn_D3DSetBlobPart ||
          !dyn_CreateBlob)
    Avail = "Missing required entry point";
  else if(!dyn_serializeRootSig && !dyn_serializeRootSigOld)
    Avail = "Missing required root signature serialize entry point";

  m_12On7 = d3d12on7;

  m_DXILSupport = (dxcompiler != NULL);

  for(int i = 0; i < argc; i++)
  {
    if(!strcmp(argv[i], "--gpuva") || !strcmp(argv[i], "--debug-gpu"))
    {
      gpuva = true;
    }
    if(i + 1 < argc &&
       (!strcmp(argv[i], "--d3d12") || !strcmp(argv[i], "--sdk") || !strcmp(argv[i], "--d3d12core")))
    {
      d3d12path = argv[i + 1];
    }
  }

  if(d3d12path.empty())
  {
    d3d12path = GetCWD() + "/D3D12/d3d12core.dll";
    FILE *f = fopen(d3d12path.c_str(), "r");
    if(!f)
      d3d12path.clear();
    else
      fclose(f);
  }

  m_Factory = factory;

  if(Avail.empty())
  {
    devFactory = PrepareCreateDeviceFromDLL(d3d12path, debugDevice, gpuva).factory;

    ID3D12DevicePtr tmpdev = CreateDevice(adapters, minFeatureLevel);

    devFactory = NULL;

    if(tmpdev)
    {
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &opts1, sizeof(opts1));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &opts2, sizeof(opts2));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &opts3, sizeof(opts3));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &opts4, sizeof(opts4));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &opts6, sizeof(opts6));
      tmpdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &opts7, sizeof(opts7));
      D3D12_FEATURE_DATA_SHADER_MODEL oShaderModel = {};
      oShaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_6;
      HRESULT hr = tmpdev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &oShaderModel,
                                               sizeof(oShaderModel));
      if(SUCCEEDED(hr))
      {
        m_HighestShaderModel = oShaderModel.HighestShaderModel;
      }
    }
  }
}

bool D3D12GraphicsTest::Init()
{
  // parse parameters here to override parameters
  if(!GraphicsTest::Init())
    return false;

  if(dyn_serializeRootSig == NULL)
  {
    TEST_WARN("Can't get D3D12SerializeVersionedRootSignature - old version of windows?");
  }

  DevicePointers devPtrs = PrepareCreateDeviceFromDLL(d3d12path, debugDevice, gpuva);

  devFactory = devPtrs.factory;
  devConfig = devPtrs.config;
  d3d12Debug = devPtrs.debug;

  dev = CreateDevice(adapters, minFeatureLevel);
  if(!dev)
    return false;

  {
    LUID luid = dev->GetAdapterLuid();

    IDXGIAdapterPtr pDXGIAdapter;
    HRESULT hr = EnumAdapterByLuid(dev->GetAdapterLuid(), pDXGIAdapter);

    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't get DXGI adapter by LUID from D3D device");
    }
    else
    {
      pDXGIAdapter->GetDesc(&adapterDesc);

      TEST_LOG("Running D3D12 test on %ls", adapterDesc.Description);
    }
  }

  PostDeviceCreate();

  if(!headless)
  {
    Win32Window *win = new Win32Window(screenWidth, screenHeight, screenTitle);

    mainWindow = win;

    DXGI_SWAP_CHAIN_DESC1 swapDesc = MakeSwapchainDesc();

    if(!d3d12on7)
    {
      IDXGIFactory4Ptr factory4 = m_Factory;

      CHECK_HR(factory4->CreateSwapChainForHwnd(queue, win->wnd, &swapDesc, NULL, NULL, &swap));

      CHECK_HR(swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&bbTex[0]));
      CHECK_HR(swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&bbTex[1]));
    }
    else
    {
      D3D12_RESOURCE_DESC bbDesc;
      bbDesc.Alignment = 0;
      bbDesc.DepthOrArraySize = 1;
      bbDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      bbDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
      bbDesc.Format = backbufferFmt;
      bbDesc.Height = screenHeight;
      bbDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      bbDesc.MipLevels = 1;
      bbDesc.SampleDesc.Count = 1;
      bbDesc.SampleDesc.Quality = 0;
      bbDesc.Width = screenWidth;

      if(bbDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
        bbDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      CHECK_HR(dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bbDesc,
                                            D3D12_RESOURCE_STATE_PRESENT, NULL,
                                            __uuidof(ID3D12Resource), (void **)&bbTex[0]));
      CHECK_HR(dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bbDesc,
                                            D3D12_RESOURCE_STATE_PRESENT, NULL,
                                            __uuidof(ID3D12Resource), (void **)&bbTex[1]));
    }
  }

  return true;
}

void D3D12GraphicsTest::PostDeviceCreate()
{
  {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = queueType;
    dev->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void **)&queue);
  }

  dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void **)&m_GPUSyncFence);
  m_GPUSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  m_GPUSyncFence->SetName(L"GPUSync fence");

  CHECK_HR(
      dev->CreateCommandAllocator(queueType, __uuidof(ID3D12CommandAllocator), (void **)&m_Alloc));

  m_Alloc->SetName(L"Command allocator");

  CHECK_HR(dev->CreateCommandList(0, queueType, m_Alloc, NULL, __uuidof(ID3D12GraphicsCommandList),
                                  (void **)&m_DebugList));

  // command buffers are allocated opened, close it immediately.
  m_DebugList->Close();

  m_DebugList->SetName(L"Debug command list");

  {
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 1;
    desc.NumDescriptors = 128;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_RTV));

    m_RTV->SetName(L"RTV heap");

    desc.NumDescriptors = 16;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_DSV));

    m_DSV->SetName(L"DSV heap");

    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    desc.NumDescriptors = 8;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_Sampler));

    m_Sampler->SetName(L"Sampler heap");

    desc.NumDescriptors = 1030;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_CBVUAVSRV));

    m_CBVUAVSRV->SetName(L"CBV/UAV/SRV heap");

    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_Clear));
    m_Clear->SetName(L"UAV clear heap");
  }

  {
    D3D12_RESOURCE_DESC readbackDesc;
    readbackDesc.Alignment = 0;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
    readbackDesc.Height = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    readbackDesc.MipLevels = 1;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.SampleDesc.Quality = 0;
    readbackDesc.Width = m_DebugBufferSize;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    CHECK_HR(dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_ReadbackBuffer));

    m_ReadbackBuffer->SetName(L"Readback buffer");

    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    CHECK_HR(dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                          D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_UploadBuffer));

    m_UploadBuffer->SetName(L"Upload buffer");
  }

  {
    std::string blitPixel = R"EOSHADER(

	Texture2D<float4> tex : register(t0);

	float4 main(float4 pos : SV_Position) : SV_Target0
	{
		return tex.Load(int3(pos.xy, 0));
	}

	)EOSHADER";

    ID3DBlobPtr vsblob = Compile(D3DFullscreenQuadVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(blitPixel, "main", "ps_4_0");

    swapBlitSig = MakeSig(
        {tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 0)});
    swapBlitPso = MakePSO().RootSig(swapBlitSig).VS(vsblob).PS(psblob);
  }

  // mute useless messages
  D3D12_MESSAGE_ID mute[] = {
      // super spammy, mostly just perf warning
      D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
      D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
  };

  infoqueue = dev;

  dev1 = dev;
  dev2 = dev;
  dev3 = dev;
  dev4 = dev;
  dev5 = dev;
  dev6 = dev;
  dev7 = dev;
  dev8 = dev;

  if(infoqueue)
  {
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = ARRAY_COUNT(mute);
    filter.DenyList.pIDList = mute;

    infoqueue->AddStorageFilterEntries(&filter);
  }

  {
    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    DefaultTriVB = MakeBuffer().Data(DefaultTri);

    DefaultTriSig = MakeSig({});

    DefaultTriPSO = MakePSO().RootSig(DefaultTriSig).InputLayout().VS(vsblob).PS(psblob);

    ResourceBarrier(DefaultTriVB, D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
  }
}

HRESULT D3D12GraphicsTest::EnumAdapterByLuid(LUID luid, IDXGIAdapterPtr &pAdapter)
{
  HRESULT hr = S_OK;

  pAdapter = NULL;

  for(UINT i = 0; i < 10; i++)
  {
    IDXGIAdapterPtr ad;
    hr = factory->EnumAdapters(i, &ad);
    if(hr == S_OK && ad)
    {
      DXGI_ADAPTER_DESC desc;
      ad->GetDesc(&desc);

      if(desc.AdapterLuid.LowPart == luid.LowPart && desc.AdapterLuid.HighPart == luid.HighPart)
      {
        pAdapter = ad;
        return S_OK;
      }
    }
    else
    {
      break;
    }
  }

  return E_FAIL;
}

std::vector<IDXGIAdapterPtr> D3D12GraphicsTest::GetAdapters()
{
  return adapters;
}

DXGI_SWAP_CHAIN_DESC1 D3D12GraphicsTest::MakeSwapchainDesc()
{
  DXGI_SWAP_CHAIN_DESC1 swapDesc = {};

  swapDesc.BufferCount = backbufferCount;
  swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  swapDesc.Flags = 0;
  swapDesc.Format = backbufferFmt;
  swapDesc.Width = screenWidth;
  swapDesc.Height = screenHeight;
  swapDesc.SampleDesc.Count = 1;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.Scaling = DXGI_SCALING_STRETCH;
  swapDesc.Stereo = FALSE;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

  return swapDesc;
}

ID3D12DevicePtr D3D12GraphicsTest::CreateDevice(const std::vector<IDXGIAdapterPtr> &adaptersToTry,
                                                D3D_FEATURE_LEVEL features)
{
  HRESULT hr = S_OK;
  for(size_t i = 0; i < adaptersToTry.size(); ++i)
  {
    ID3D12DevicePtr device;

    if(devFactory)
      hr = devFactory->CreateDevice(adaptersToTry[i], features, __uuidof(ID3D12Device),
                                    (void **)&device);
    else
      hr = dyn_D3D12CreateDevice(adaptersToTry[i], features, __uuidof(ID3D12Device),
                                 (void **)&device);

    if(SUCCEEDED(hr))
      return device;
  }

  TEST_ERROR("D3D12CreateDevice failed: %x", hr);
  return NULL;
}

GraphicsWindow *D3D12GraphicsTest::MakeWindow(int width, int height, const char *title)
{
  return new Win32Window(width, height, title);
}

void D3D12GraphicsTest::Shutdown()
{
  GPUSync();

  infoqueue = NULL;

  pendingCommandBuffers.clear();
  freeCommandBuffers.clear();

  m_ReadbackBuffer = NULL;
  m_UploadBuffer = NULL;

  m_RTV = m_DSV = m_CBVUAVSRV = m_Sampler = NULL;

  m_Alloc = NULL;
  m_DebugList = NULL;

  m_GPUSyncFence = NULL;
  CloseHandle(m_GPUSyncHandle);

  bbTex[0] = bbTex[1] = NULL;

  swap = NULL;
  m_Factory = NULL;
  delete mainWindow;

  queue = NULL;
  dev = NULL;
  devConfig = NULL;
}

bool D3D12GraphicsTest::Running()
{
  if(!FrameLimit())
    return false;

  return mainWindow->Update();
}

ID3D12ResourcePtr D3D12GraphicsTest::StartUsingBackbuffer(ID3D12GraphicsCommandListPtr cmd,
                                                          D3D12_RESOURCE_STATES useState)
{
  ID3D12ResourcePtr bb = bbTex[texIdx];

  if(useState != D3D12_RESOURCE_STATE_PRESENT)
    ResourceBarrier(cmd, bbTex[texIdx], D3D12_RESOURCE_STATE_PRESENT, useState);

  BBRTV = MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(31);

  return bbTex[texIdx];
}

void D3D12GraphicsTest::FinishUsingBackbuffer(ID3D12GraphicsCommandListPtr cmd,
                                              D3D12_RESOURCE_STATES usedState)
{
  ID3D12ResourcePtr bb = bbTex[texIdx];

  if(usedState != D3D12_RESOURCE_STATE_PRESENT)
    ResourceBarrier(cmd, bbTex[texIdx], usedState, D3D12_RESOURCE_STATE_PRESENT);

  texIdx = 1 - texIdx;
}

void D3D12GraphicsTest::Submit(const std::vector<ID3D12GraphicsCommandListPtr> &cmds)
{
  std::vector<ID3D12CommandList *> submits;

  m_GPUSyncCounter++;

  for(const ID3D12GraphicsCommandListPtr &cmd : cmds)
  {
    pendingCommandBuffers.push_back(std::make_pair(cmd, m_GPUSyncCounter));
    submits.push_back(cmd);
  }

  queue->ExecuteCommandLists((UINT)submits.size(), submits.data());
  queue->Signal(m_GPUSyncFence, m_GPUSyncCounter);
}

void D3D12GraphicsTest::GPUSync()
{
  m_GPUSyncCounter++;

  CHECK_HR(queue->Signal(m_GPUSyncFence, m_GPUSyncCounter));
  CHECK_HR(m_GPUSyncFence->SetEventOnCompletion(m_GPUSyncCounter, m_GPUSyncHandle));
  WaitForSingleObject(m_GPUSyncHandle, 10000);
}

void D3D12GraphicsTest::SubmitAndPresent(const std::vector<ID3D12GraphicsCommandListPtr> &cmds)
{
  Submit(cmds);
  Present();
}

void D3D12GraphicsTest::Present()
{
  if(swap)
  {
    swap->Present(0, 0);
  }
  else
  {
    ID3D12CommandQueueDownlevelPtr downlevel = queue;

    ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
    Reset(cmd);

    downlevel->Present(cmd, bbTex[1 - texIdx], ((Win32Window *)mainWindow)->wnd,
                       D3D12_DOWNLEVEL_PRESENT_FLAG_NONE);

    m_GPUSyncCounter++;
    queue->Signal(m_GPUSyncFence, m_GPUSyncCounter);

    pendingCommandBuffers.push_back(std::make_pair(cmd, m_GPUSyncFence));
  }

  for(auto it = pendingCommandBuffers.begin(); it != pendingCommandBuffers.end();)
  {
    if(m_GPUSyncFence->GetCompletedValue() >= it->second)
    {
      freeCommandBuffers.push_back(it->first);
      it = pendingCommandBuffers.erase(it);
    }
    else
    {
      ++it;
    }
  }

  GPUSync();

  m_Alloc->Reset();
}

void D3D12GraphicsTest::AddHashIfMissing(void *ByteCode, size_t BytecodeLength)
{
  struct FileHeader
  {
    uint32_t fourcc;
    uint32_t hashValue[4];
    uint32_t containerVersion;
    uint32_t fileLength;
  };

  if(BytecodeLength < sizeof(FileHeader))
  {
    TEST_ERROR("Trying to hash corrupt DXBC container");
    return;
  }

  FileHeader *header = (FileHeader *)ByteCode;

#define MAKE_FOURCC(a, b, c, d) \
  (((uint32_t)(d) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a))

  if(header->fourcc != MAKE_FOURCC('D', 'X', 'B', 'C'))
  {
    TEST_ERROR("Trying to hash corrupt DXBC container");
    return;
  }

  if(header->fileLength != (uint32_t)BytecodeLength)
  {
    TEST_ERROR("Trying to hash corrupt DXBC container");
    return;
  }

  if(header->hashValue[0] != 0 || header->hashValue[1] != 0 || header->hashValue[2] != 0 ||
     header->hashValue[3] != 0)
    return;

  MD5_CTX md5ctx = {};
  MD5_Init(&md5ctx);

  // the hashable data starts immediately after the hash.
  byte *data = (byte *)&header->containerVersion;
  uint32_t length = uint32_t(BytecodeLength - offsetof(FileHeader, containerVersion));

  // we need to know the number of bits for putting in the trailing padding.
  uint32_t numBits = length * 8;
  uint32_t numBitsPart2 = (numBits >> 2) | 1;

  // MD5 works on 64-byte chunks, process the first set of whole chunks, leaving 0-63 bytes left
  // over
  uint32_t leftoverLength = length % 64;
  MD5_Update(&md5ctx, data, length - leftoverLength);

  data += length - leftoverLength;

  uint32_t block[16] = {};
  static_assert(sizeof(block) == 64, "Block is not properly sized for MD5 round");

  // normally MD5 finishes by appending a 1 bit to the bitstring. Since we are only appending bytes
  // this would be an 0x80 byte (the first bit is considered to be the MSB). Then it pads out with
  // zeroes until it has 56 bytes in the last block and appends appends the message length as a
  // 64-bit integer as the final part of that block.
  // in other words, normally whatever is leftover from the actual message gets one byte appended,
  // then if there's at least 8 bytes left we'll append the length. Otherwise we pad that block with
  // 0s and create a new block with the length at the end.
  // Or as the original RFC/spec says: padding is always performed regardless of whether the
  // original buffer already ended in exactly a 56 byte block.
  //
  // The DXBC finalisation is slightly different (previous work suggests this is due to a bug in the
  // original implementation and it was maybe intended to be exactly MD5?):
  //
  // The length provided in the padding block is not 64-bit properly: the second dword with the high
  // bits is instead the number of nybbles(?) with 1 OR'd on. The length is also split, so if it's
  // in
  // a padding block the low bits are in the first dword and the upper bits in the last. If there's
  // no padding block the low dword is passed in first before the leftovers of the message and then
  // the upper bits at the end.

  // if the leftovers uses at least 56, we can't fit both the trailing 1 and the 64-bit length, so
  // we need a padding block and then our own block for the length.
  if(leftoverLength >= 56)
  {
    // pass in the leftover data padded out to 64 bytes with zeroes
    MD5_Update(&md5ctx, data, leftoverLength);

    block[0] = 0x80;    // first padding bit is 1
    MD5_Update(&md5ctx, block, 64 - leftoverLength);

    // the final block contains the number of bits in the first dword, and the weird upper bits
    block[0] = numBits;
    block[15] = numBitsPart2;

    // process this block directly, we're replacing the call to MD5_Final here manually
    MD5_Update(&md5ctx, block, 64);
  }
  else
  {
    // the leftovers mean we can put the padding inside the final block. But first we pass the "low"
    // number of bits:
    MD5_Update(&md5ctx, &numBits, sizeof(numBits));

    if(leftoverLength)
      MD5_Update(&md5ctx, data, leftoverLength);

    uint32_t paddingBytes = 64 - leftoverLength - 4;

    // prepare the remainder of this block, starting with the 0x80 padding start right after the
    // leftovers and the first part of the bit length above.
    block[0] = 0x80;
    // then add the remainder of the 'length' here in the final part of the block
    memcpy(((byte *)block) + paddingBytes - 4, &numBitsPart2, 4);

    MD5_Update(&md5ctx, block, paddingBytes);
  }

  header->hashValue[0] = md5ctx.a;
  header->hashValue[1] = md5ctx.b;
  header->hashValue[2] = md5ctx.c;
  header->hashValue[3] = md5ctx.d;
}

std::vector<byte> D3D12GraphicsTest::GetBufferData(ID3D12ResourcePtr buffer,
                                                   D3D12_RESOURCE_STATES state, uint32_t offset,
                                                   uint64_t length)
{
  std::vector<byte> ret;

  if(buffer == NULL)
    return ret;

  D3D12_RESOURCE_DESC desc = buffer->GetDesc();
  D3D12_HEAP_PROPERTIES heapProps;
  buffer->GetHeapProperties(&heapProps, NULL);

  if(offset >= desc.Width)
  {
    TEST_ERROR("Out of bounds offset passed to GetBufferData");
    // can't read past the end of the buffer, return empty
    return ret;
  }

  if(length == 0)
  {
    length = desc.Width - offset;
  }

  if(length > 0 && offset + length > desc.Width)
  {
    TEST_WARN("Attempting to read off the end of the array. Will be clamped");
    length = std::min(length, desc.Width - offset);
  }

  uint64_t outOffs = 0;

  ret.resize((size_t)length);

  // directly CPU mappable (and possibly invalid to transition and copy from), so just memcpy
  if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD || heapProps.Type == D3D12_HEAP_TYPE_READBACK)
  {
    D3D12_RANGE range = {(size_t)offset, size_t(offset + length)};

    byte *data = NULL;
    CHECK_HR(buffer->Map(0, &range, (void **)&data));

    memcpy(&ret[0], data + offset, (size_t)length);

    range.Begin = range.End = 0;

    buffer->Unmap(0, &range);

    return ret;
  }

  m_DebugList->Reset(m_Alloc, NULL);

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = buffer;
  barrier.Transition.StateBefore = state;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  if(barrier.Transition.StateBefore != barrier.Transition.StateAfter)
    m_DebugList->ResourceBarrier(1, &barrier);

  while(length > 0)
  {
    uint64_t chunkSize = std::min(length, m_DebugBufferSize);

    m_DebugList->CopyBufferRegion(m_ReadbackBuffer, 0, buffer, offset, chunkSize);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    queue->ExecuteCommandLists(1, &l);

    GPUSync();

    m_Alloc->Reset();

    D3D12_RANGE range = {0, (size_t)chunkSize};

    void *data = NULL;
    CHECK_HR(m_ReadbackBuffer->Map(0, &range, &data));

    memcpy(&ret[(size_t)outOffs], data, (size_t)chunkSize);

    range.End = 0;

    m_ReadbackBuffer->Unmap(0, &range);

    outOffs += chunkSize;
    length -= chunkSize;

    m_DebugList->Reset(m_Alloc, NULL);
  }

  if(barrier.Transition.StateBefore != barrier.Transition.StateAfter)
  {
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

    m_DebugList->ResourceBarrier(1, &barrier);
  }

  m_DebugList->Close();

  ID3D12CommandList *l = m_DebugList;
  queue->ExecuteCommandLists(1, &l);
  GPUSync();
  m_Alloc->Reset();

  return ret;
}

void D3D12GraphicsTest::SetBufferData(ID3D12ResourcePtr buffer, D3D12_RESOURCE_STATES state,
                                      const byte *data, uint64_t len)
{
  D3D12_RESOURCE_DESC desc = buffer->GetDesc();
  D3D12_HEAP_PROPERTIES heapProps;
  buffer->GetHeapProperties(&heapProps, NULL);

  if(len > desc.Width)
  {
    TEST_ERROR("Can't upload more data than buffer contains");
    return;
  }

  // directly CPU mappable (and possibly invalid to transition and copy from), so just memcpy
  if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD || heapProps.Type == D3D12_HEAP_TYPE_READBACK)
  {
    D3D12_RANGE range = {0, 0};

    byte *ptr = NULL;
    CHECK_HR(buffer->Map(0, &range, (void **)&ptr));

    memcpy(ptr, data, (size_t)len);

    range.End = (size_t)len;

    buffer->Unmap(0, &range);

    return;
  }

  m_DebugList->Reset(m_Alloc, NULL);

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = buffer;
  barrier.Transition.StateBefore = state;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

  if(barrier.Transition.StateBefore != barrier.Transition.StateAfter)
    m_DebugList->ResourceBarrier(1, &barrier);

  uint64_t offset = 0;

  while(len > 0)
  {
    uint64_t chunkSize = std::min(len, m_DebugBufferSize);

    {
      D3D12_RANGE range = {0, 0};

      void *ptr = NULL;
      CHECK_HR(m_UploadBuffer->Map(0, &range, &ptr));

      memcpy(ptr, data + offset, (size_t)chunkSize);

      range.End = (size_t)chunkSize;

      m_UploadBuffer->Unmap(0, &range);
    }

    m_DebugList->CopyBufferRegion(buffer, offset, m_UploadBuffer, 0, chunkSize);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    queue->ExecuteCommandLists(1, &l);

    GPUSync();

    m_Alloc->Reset();

    offset += chunkSize;
    len -= chunkSize;

    m_DebugList->Reset(m_Alloc, NULL);
  }

  if(barrier.Transition.StateBefore != barrier.Transition.StateAfter)
  {
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

    m_DebugList->ResourceBarrier(1, &barrier);
  }

  m_DebugList->Close();

  ID3D12CommandList *l = m_DebugList;
  queue->ExecuteCommandLists(1, &l);
  GPUSync();
  m_Alloc->Reset();
}

void D3D12GraphicsTest::pushMarker(ID3D12GraphicsCommandListPtr cmd, const std::string &name)
{
  // D3D debug layer spams un-mutable errors if we don't include the NULL terminator in the size.
  cmd->BeginEvent(1, name.data(), (UINT)name.size() + 1);
}

void D3D12GraphicsTest::setMarker(ID3D12GraphicsCommandListPtr cmd, const std::string &name)
{
  cmd->SetMarker(1, name.data(), (UINT)name.size() + 1);
}

void D3D12GraphicsTest::popMarker(ID3D12GraphicsCommandListPtr cmd)
{
  cmd->EndEvent();
}

void D3D12GraphicsTest::blitToSwap(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr src,
                                   ID3D12ResourcePtr dst, DXGI_FORMAT srvFormat)
{
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = MakeRTV(dst).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  cmd->SetPipelineState(swapBlitPso);
  cmd->SetGraphicsRootSignature(swapBlitSig);

  static UINT idx = 0;
  idx++;
  idx %= 6;

  D3D12_GPU_DESCRIPTOR_HANDLE handle;
  if(srvFormat == DXGI_FORMAT_UNKNOWN)
    handle = MakeSRV(src).CreateGPU(1024 + idx);
  else
    handle = MakeSRV(src).Format(srvFormat).CreateGPU(1024 + idx);

  cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
  cmd->SetGraphicsRootDescriptorTable(0, handle);

  RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
  RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

  OMSetRenderTargets(cmd, {rtv}, {});

  cmd->DrawInstanced(4, 1, 0, 0);
}

void D3D12GraphicsTest::ResourceBarrier(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr res,
                                        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
  D3D12_RESOURCE_BARRIER barrier;
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = res;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = before;
  barrier.Transition.StateAfter = after;
  cmd->ResourceBarrier(1, &barrier);
}

void D3D12GraphicsTest::ResourceBarrier(ID3D12ResourcePtr res, D3D12_RESOURCE_STATES before,
                                        D3D12_RESOURCE_STATES after)

{
  ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

  Reset(cmd);
  ResourceBarrier(cmd, res, before, after);
  cmd->Close();

  Submit({cmd});
}

void D3D12GraphicsTest::IASetVertexBuffer(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr vb,
                                          UINT stride, UINT offset)
{
  D3D12_VERTEX_BUFFER_VIEW view;
  view.BufferLocation = vb->GetGPUVirtualAddress() + offset;
  view.SizeInBytes = UINT(vb->GetDesc().Width - offset);
  view.StrideInBytes = stride;
  cmd->IASetVertexBuffers(0, 1, &view);
}

void D3D12GraphicsTest::IASetIndexBuffer(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr ib,
                                         DXGI_FORMAT fmt, UINT offset)
{
  D3D12_INDEX_BUFFER_VIEW view;
  view.BufferLocation = ib->GetGPUVirtualAddress() + offset;
  view.Format = fmt;
  view.SizeInBytes = UINT(ib->GetDesc().Width - offset);
  cmd->IASetIndexBuffer(&view);
}

void D3D12GraphicsTest::ClearRenderTargetView(ID3D12GraphicsCommandListPtr cmd,
                                              ID3D12ResourcePtr rt, Vec4f col)
{
  cmd->ClearRenderTargetView(MakeRTV(rt).CreateCPU(0), &col.x, 0, NULL);
}

void D3D12GraphicsTest::ClearRenderTargetView(ID3D12GraphicsCommandListPtr cmd,
                                              D3D12_CPU_DESCRIPTOR_HANDLE rt, Vec4f col)
{
  cmd->ClearRenderTargetView(rt, &col.x, 0, NULL);
}

void D3D12GraphicsTest::ClearDepthStencilView(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr dsv,
                                              D3D12_CLEAR_FLAGS flags, float depth, UINT8 stencil)
{
  MakeDSV(dsv).CreateCPU(0);
  cmd->ClearDepthStencilView(m_DSV->GetCPUDescriptorHandleForHeapStart(), flags, depth, stencil, 0,
                             NULL);
}

void D3D12GraphicsTest::ClearDepthStencilView(ID3D12GraphicsCommandListPtr cmd,
                                              D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                                              D3D12_CLEAR_FLAGS flags, float depth, UINT8 stencil)
{
  cmd->ClearDepthStencilView(dsv, flags, depth, stencil, 0, NULL);
}

void D3D12GraphicsTest::RSSetViewport(ID3D12GraphicsCommandListPtr cmd, D3D12_VIEWPORT view)
{
  cmd->RSSetViewports(1, &view);
}

void D3D12GraphicsTest::RSSetScissorRect(ID3D12GraphicsCommandListPtr cmd, D3D12_RECT rect)
{
  cmd->RSSetScissorRects(1, &rect);
}

void D3D12GraphicsTest::SetMainWindowViewScissor(ID3D12GraphicsCommandListPtr cmd)
{
  RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
  RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});
}

void D3D12GraphicsTest::OMSetRenderTargets(ID3D12GraphicsCommandListPtr cmd,
                                           const std::vector<ID3D12ResourcePtr> &rtvs,
                                           ID3D12ResourcePtr dsv)
{
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
  handles.resize(rtvs.size());
  for(size_t i = 0; i < rtvs.size(); i++)
    handles[i] = MakeRTV(rtvs[i]).CreateCPU((uint32_t)i);

  if(dsv)
    OMSetRenderTargets(cmd, handles, MakeDSV(dsv).CreateCPU(0));
  else
    OMSetRenderTargets(cmd, handles, {});
}

void D3D12GraphicsTest::OMSetRenderTargets(ID3D12GraphicsCommandListPtr cmd,
                                           const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> &rtvs,
                                           D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
  cmd->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), FALSE, dsv.ptr ? &dsv : NULL);
}

COM_SMARTPTR(IDxcLibrary);
COM_SMARTPTR(IDxcCompiler);
COM_SMARTPTR(IDxcBlobEncoding);
COM_SMARTPTR(IDxcOperationResult);
COM_SMARTPTR(IDxcBlob);

ID3DBlobPtr D3D12GraphicsTest::Compile(std::string src, std::string entry, std::string profile,
                                       bool skipoptimise)
{
  ID3DBlobPtr blob = NULL;

  if(profile[3] >= '6')
  {
    if(!m_DXILSupport)
      TEST_FATAL("Can't compile DXIL shader");

    pDxcCreateInstance dxcCreate =
        (pDxcCreateInstance)GetProcAddress(dxcompiler, "DxcCreateInstance");

    IDxcLibraryPtr library = NULL;
    HRESULT hr = dxcCreate(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void **)&library);

    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't create DXC library");
      return NULL;
    }

    IDxcCompilerPtr compiler = NULL;
    hr = dxcCreate(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void **)&compiler);

    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't create DXC compiler");
      return NULL;
    }

    IDxcBlobEncodingPtr sourceBlob = NULL;
    hr = library->CreateBlobWithEncodingFromPinned(src.data(), (UINT)src.size(), CP_UTF8,
                                                   &sourceBlob);

    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't create DXC blob");
      return NULL;
    }

    const size_t numAttempts = 2;
    std::vector<const wchar_t *> args[numAttempts];
    std::vector<std::wstring> argStorage;

    argStorage.push_back(L"-WX");
    if(skipoptimise)
    {
      argStorage.push_back(L"-O0");
      argStorage.push_back(L"-Od");
    }
    else
    {
      argStorage.push_back(L"-Ges");
      argStorage.push_back(L"-O1");
    }
    argStorage.push_back(L"-Zi");
    argStorage.push_back(L"-Qembed_debug");

    for(size_t i = 0; i < argStorage.size(); i++)
      args[0].push_back(argStorage[i].c_str());

    // The second set of args excludes -Qembed_debug, which can fail on older Windows 10 SDKs
    for(size_t i = 0; i < argStorage.size() - 1; i++)
      args[1].push_back(argStorage[i].c_str());

    IDxcOperationResultPtr result;
    HRESULT hrStatus;
    for(size_t i = 0; i < numAttempts; ++i)
    {
      result = NULL;
      hrStatus = E_NOINTERFACE;

      hr = compiler->Compile(sourceBlob, UTF82Wide(entry).c_str(), UTF82Wide(entry).c_str(),
                             UTF82Wide(profile).c_str(), args[i].data(), (UINT)args[i].size(), NULL,
                             0, NULL, &result);

      if(result)
        result->GetStatus(&hrStatus);

      // Break early if compiling succeeds
      if(SUCCEEDED(hr) && SUCCEEDED(hrStatus))
        break;
    }

    if(SUCCEEDED(hr) && SUCCEEDED(hrStatus))
    {
      IDxcBlobPtr code = NULL;
      result->GetResult(&code);

      dyn_CreateBlob((uint32_t)code->GetBufferSize(), &blob);

      memcpy(blob->GetBufferPointer(), code->GetBufferPointer(), code->GetBufferSize());

      // if we didn't have dxil.dll around there won't be a hash, add it ourselves
      AddHashIfMissing(blob->GetBufferPointer(), code->GetBufferSize());
    }
    else
    {
      if(result)
      {
        IDxcBlobEncodingPtr dxcErrors = NULL;
        hr = result->GetErrorBuffer(&dxcErrors);
        if(SUCCEEDED(hr) && dxcErrors)
        {
          TEST_ERROR("Failed to compile DXC shader: %s", dxcErrors->GetBufferPointer());
        }
        else
        {
          TEST_ERROR("DXC compile failed but couldn't get error: %x", hr);
        }
      }
      else
      {
        TEST_ERROR("No compilation result found from DXC compile: %x", hr);
      }
    }
  }
  else
  {
    ID3DBlobPtr error = NULL;

    UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_DEBUG |
                 D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

    if(skipoptimise)
      flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_OPTIMIZATION_LEVEL0;
    else
      flags |= D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL0;

    HRESULT hr = dyn_D3DCompile(src.c_str(), src.length(), "", NULL, NULL, entry.c_str(),
                                profile.c_str(), flags, 0, &blob, &error);

    if(FAILED(hr))
    {
      TEST_ERROR("Failed to compile shader, error %x / %s", hr,
                 error ? (char *)error->GetBufferPointer() : "Unknown");
      return NULL;
    }
  }

  return blob;
}

void D3D12GraphicsTest::WriteBlob(std::string name, ID3DBlobPtr blob, bool compress)
{
  FILE *f = NULL;
  fopen_s(&f, name.c_str(), "wb");

  if(f == NULL)
  {
    TEST_ERROR("Can't open blob file to write %s", name.c_str());
    return;
  }

  if(compress)
  {
    int uncompSize = (int)blob->GetBufferSize();
    char *compBuf = new char[uncompSize];

    int compressedSize = LZ4_compress_default((const char *)blob->GetBufferPointer(), compBuf,
                                              uncompSize, uncompSize);

    fwrite(compBuf, 1, compressedSize, f);

    delete[] compBuf;
  }
  else
  {
    fwrite(blob->GetBufferPointer(), 1, blob->GetBufferSize(), f);
  }

  fclose(f);
}

void D3D12GraphicsTest::SetBlobPath(std::string name, ID3DBlobPtr &blob)
{
  ID3DBlobPtr newBlob = NULL;

  const GUID RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

  std::string pathData;
  for(size_t i = 0; i < sizeof(RENDERDOC_ShaderDebugMagicValue); i++)
    pathData.push_back(' ');

  pathData += name;

  memcpy(&pathData[0], &RENDERDOC_ShaderDebugMagicValue, sizeof(RENDERDOC_ShaderDebugMagicValue));

  dyn_D3DSetBlobPart(blob->GetBufferPointer(), blob->GetBufferSize(), D3D_BLOB_PRIVATE_DATA, 0,
                     pathData.c_str(), pathData.size() + 1, &newBlob);

  blob = newBlob;
}

void D3D12GraphicsTest::SetBlobPath(std::string name, ID3D12DeviceChild *shader)
{
  const GUID RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

  shader->SetPrivateData(RENDERDOC_ShaderDebugMagicValue, (UINT)name.size() + 1, name.c_str());
}

ID3D12GraphicsCommandListPtr D3D12GraphicsTest::GetCommandBuffer()
{
  if(freeCommandBuffers.empty())
  {
    ID3D12GraphicsCommandListPtr list = NULL;
    CHECK_HR(dev->CreateCommandList(0, queueType, m_Alloc, NULL,
                                    __uuidof(ID3D12GraphicsCommandList), (void **)&list));
    // list starts opened, close it
    list->Close();
    freeCommandBuffers.push_back(list);
  }

  ID3D12GraphicsCommandListPtr ret = freeCommandBuffers.back();
  freeCommandBuffers.pop_back();

  return ret;
}

void D3D12GraphicsTest::Reset(ID3D12GraphicsCommandListPtr cmd)
{
  cmd->Reset(m_Alloc, NULL);
}

ID3D12RootSignaturePtr D3D12GraphicsTest::MakeSig(const std::vector<D3D12_ROOT_PARAMETER1> &params,
                                                  D3D12_ROOT_SIGNATURE_FLAGS Flags,
                                                  UINT NumStaticSamplers,
                                                  const D3D12_STATIC_SAMPLER_DESC *StaticSamplers)
{
  ID3DBlobPtr blob;

  if(dyn_serializeRootSig == NULL)
  {
    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.Flags = Flags;
    desc.NumStaticSamplers = NumStaticSamplers;
    desc.pStaticSamplers = StaticSamplers;
    desc.NumParameters = (UINT)params.size();

    std::vector<D3D12_ROOT_PARAMETER> params_1_0;
    params_1_0.resize(params.size());
    for(size_t i = 0; i < params.size(); i++)
    {
      params_1_0[i].ShaderVisibility = params[i].ShaderVisibility;
      params_1_0[i].ParameterType = params[i].ParameterType;

      if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      {
        params_1_0[i].Constants = params[i].Constants;
      }
      else if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        params_1_0[i].DescriptorTable.NumDescriptorRanges =
            params[i].DescriptorTable.NumDescriptorRanges;

        D3D12_DESCRIPTOR_RANGE *dst =
            new D3D12_DESCRIPTOR_RANGE[params[i].DescriptorTable.NumDescriptorRanges];
        params_1_0[i].DescriptorTable.pDescriptorRanges = dst;

        for(UINT r = 0; r < params[i].DescriptorTable.NumDescriptorRanges; r++)
        {
          dst[r].BaseShaderRegister =
              params[i].DescriptorTable.pDescriptorRanges[r].BaseShaderRegister;
          dst[r].NumDescriptors = params[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
          dst[r].OffsetInDescriptorsFromTableStart =
              params[i].DescriptorTable.pDescriptorRanges[r].OffsetInDescriptorsFromTableStart;
          dst[r].RangeType = params[i].DescriptorTable.pDescriptorRanges[r].RangeType;
          dst[r].RegisterSpace = params[i].DescriptorTable.pDescriptorRanges[r].RegisterSpace;

          if(params[i].DescriptorTable.pDescriptorRanges[r].Flags !=
             (D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE))
            TEST_WARN("Losing information when reducing down to 1.0 root signature");
        }
      }
      else
      {
        params_1_0[i].Descriptor.RegisterSpace = params[i].Descriptor.RegisterSpace;
        params_1_0[i].Descriptor.ShaderRegister = params[i].Descriptor.ShaderRegister;

        if(params[i].Descriptor.Flags != D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE)
          TEST_WARN("Losing information when reducing down to 1.0 root signature");
      }
    }

    desc.pParameters = &params_1_0[0];

    ID3DBlobPtr errBlob;
    HRESULT hr = dyn_serializeRootSigOld(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errBlob);

    for(size_t i = 0; i < params_1_0.size(); i++)
      if(params_1_0[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        delete[] params_1_0[i].DescriptorTable.pDescriptorRanges;

    if(FAILED(hr))
    {
      std::string errors = (char *)errBlob->GetBufferPointer();

      std::string logerror = errors;
      if(logerror.length() > 1024)
        logerror = logerror.substr(0, 1024) + "...";

      TEST_ERROR("Root signature serialize error:\n%s", logerror.c_str());

      return NULL;
    }
  }
  else
  {
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC verdesc;
    verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

    D3D12_ROOT_SIGNATURE_DESC1 &desc = verdesc.Desc_1_1;
    desc.Flags = Flags;
    desc.NumStaticSamplers = NumStaticSamplers;
    desc.pStaticSamplers = StaticSamplers;
    desc.NumParameters = (UINT)params.size();
    desc.pParameters = &params[0];

    ID3DBlobPtr errBlob = NULL;
    HRESULT hr;
    if(devConfig)
      hr = devConfig->SerializeVersionedRootSignature(&verdesc, &blob, &errBlob);
    else
      hr = dyn_serializeRootSig(&verdesc, &blob, &errBlob);

    if(FAILED(hr))
    {
      std::string errors = (char *)errBlob->GetBufferPointer();

      std::string logerror = errors;
      if(logerror.length() > 1024)
        logerror = logerror.substr(0, 1024) + "...";

      TEST_ERROR("Root signature serialize error:\n%s", logerror.c_str());

      return NULL;
    }
  }

  if(!blob)
    return NULL;

  ID3D12RootSignaturePtr ret;
  CHECK_HR(dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                    __uuidof(ID3D12RootSignature), (void **)&ret));
  return ret;
}

ID3D12CommandSignaturePtr D3D12GraphicsTest::MakeCommandSig(
    ID3D12RootSignaturePtr rootSig, const std::vector<D3D12_INDIRECT_ARGUMENT_DESC> &params)
{
  D3D12_COMMAND_SIGNATURE_DESC desc = {};
  desc.pArgumentDescs = params.data();
  desc.NumArgumentDescs = (UINT)params.size();

  for(const D3D12_INDIRECT_ARGUMENT_DESC &p : params)
  {
    switch(p.Type)
    {
      case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
        desc.ByteStride += sizeof(D3D12_DRAW_ARGUMENTS);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
        desc.ByteStride += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
        desc.ByteStride += sizeof(D3D12_DISPATCH_ARGUMENTS);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
        desc.ByteStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
        desc.ByteStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        desc.ByteStride += p.Constant.Num32BitValuesToSet * sizeof(uint32_t);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        desc.ByteStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        desc.ByteStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
        break;
      case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
        desc.ByteStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
        break;
    }
  }

  ID3D12CommandSignaturePtr ret;
  CHECK_HR(
      dev->CreateCommandSignature(&desc, rootSig, __uuidof(ID3D12CommandSignature), (void **)&ret));
  return ret;
}
