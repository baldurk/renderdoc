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

#include "d3d11_test.h"
#include <stdio.h>
#include "../3rdparty/lz4/lz4.h"
#include "../renderdoc_app.h"
#include "../win32/win32_window.h"

typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY)(REFIID, void **);

namespace
{
HMODULE d3d11 = NULL;
HMODULE dxgi = NULL;
HMODULE d3dcompiler = NULL;
IDXGIFactory1Ptr factory;
std::vector<IDXGIAdapterPtr> adapters;
bool warp = false;

pD3DCompile dyn_D3DCompile = NULL;
pD3DStripShader dyn_D3DStripShader = NULL;
pD3DSetBlobPart dyn_D3DSetBlobPart = NULL;

PFN_D3D11_CREATE_DEVICE dyn_D3D11CreateDevice = NULL;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN dyn_D3D11CreateDeviceAndSwapChain = NULL;
};

void D3D11GraphicsTest::Prepare(int argc, char **argv)
{
  GraphicsTest::Prepare(argc, argv);

  static bool prepared = false;

  if(!prepared)
  {
    prepared = true;

    d3d11 = LoadLibraryA("d3d11.dll");
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

    PFN_CREATE_DXGI_FACTORY createFactory = NULL;

    if(dxgi)
    {
      createFactory = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(dxgi, "CreateDXGIFactory1");
    }

    if(d3d11 && d3dcompiler)
    {
      dyn_D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11, "D3D11CreateDevice");
      dyn_D3D11CreateDeviceAndSwapChain = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
          d3d11, "D3D11CreateDeviceAndSwapChain");

      dyn_D3DCompile = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");
      dyn_D3DStripShader = (pD3DStripShader)GetProcAddress(d3dcompiler, "D3DStripShader");
      dyn_D3DSetBlobPart = (pD3DSetBlobPart)GetProcAddress(d3dcompiler, "D3DSetBlobPart");
    }

    HRESULT hr = S_OK;

    if(createFactory)
    {
      hr = createFactory(__uuidof(IDXGIFactory1), (void **)&factory);

      if(SUCCEEDED(hr))
        adapters = FindD3DAdapters(factory, argc, argv, warp);
    }
  }

  if(!d3d11)
    Avail = "d3d11.dll is not available";
  else if(!dxgi)
    Avail = "dxgi.dll is not available";
  else if(!d3dcompiler)
    Avail = "d3dcompiler_XX.dll is not available";
  else if(!factory)
    Avail = "Couldn't create DXGI factory";
  else if(!dyn_D3D11CreateDevice || !dyn_D3D11CreateDeviceAndSwapChain || !dyn_D3DCompile ||
          !dyn_D3DStripShader || !dyn_D3DSetBlobPart)
    Avail = "Missing required entry point";

  if(dyn_D3D11CreateDevice)
  {
    D3D_FEATURE_LEVEL features[] = {D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = CreateDevice(NULL, NULL, features, 0);

    if(SUCCEEDED(hr))
    {
      dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &opts, sizeof(opts));
      dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS1, &opts1, sizeof(opts1));
      dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &opts2, sizeof(opts2));
    }

    // This device was only used  to get feature support. Set it back to NULL
    dev = NULL;
  }
}

bool D3D11GraphicsTest::Init(IDXGIAdapterPtr pAdapter)
{
  if(!GraphicsTest::Init())
    return false;

  D3D_FEATURE_LEVEL features[] = {feature_level};

  HRESULT hr = S_OK;

  UINT flags = createFlags | (debugDevice ? D3D11_CREATE_DEVICE_DEBUG : 0);

  if(headless)
  {
    hr = CreateDevice(pAdapter, NULL, features, flags);

    if(FAILED(hr))
    {
      TEST_ERROR("D3D11CreateDevice failed: %x", hr);
      return false;
    }

    PostDeviceCreate();
    return true;
  }

  Win32Window *win = new Win32Window(screenWidth, screenHeight, screenTitle);

  mainWindow = win;

  DXGI_SWAP_CHAIN_DESC swapDesc = MakeSwapchainDesc(win);

  hr = CreateDevice(pAdapter, &swapDesc, features, flags);

  if(FAILED(hr))
  {
    TEST_ERROR("D3D11CreateDeviceAndSwapChain failed: %x", hr);
    return false;
  }

  hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bbTex);

  if(FAILED(hr))
  {
    TEST_ERROR("swap->GetBuffer failed: %x", hr);
    dev = NULL;
    ctx = NULL;
    swap = NULL;
    return false;
  }

  hr = dev->CreateRenderTargetView(bbTex, NULL, &bbRTV);

  if(FAILED(hr))
  {
    TEST_ERROR("CreateRenderTargetView failed: %x", hr);
    return false;
  }

  PostDeviceCreate();

  return true;
}

DXGI_SWAP_CHAIN_DESC D3D11GraphicsTest::MakeSwapchainDesc(GraphicsWindow *win)
{
  DXGI_SWAP_CHAIN_DESC swapDesc = {};

  swapDesc.BufferCount = backbufferCount;
  swapDesc.BufferDesc.Format = backbufferFmt;
  swapDesc.BufferDesc.Width = screenWidth;
  swapDesc.BufferDesc.Height = screenHeight;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  swapDesc.SampleDesc.Count = backbufferMSAA;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.OutputWindow = ((Win32Window *)win)->wnd;
  swapDesc.Windowed = TRUE;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  swapDesc.Flags = 0;

  return swapDesc;
}

GraphicsWindow *D3D11GraphicsTest::MakeWindow(int width, int height, const char *title)
{
  return new Win32Window(width, height, title);
}

std::vector<IDXGIAdapterPtr> D3D11GraphicsTest::GetAdapters()
{
  return adapters;
}

HRESULT D3D11GraphicsTest::CreateDevice(IDXGIAdapterPtr adapterToTry, DXGI_SWAP_CHAIN_DESC *swapDesc,
                                        D3D_FEATURE_LEVEL *features, UINT flags)
{
  HRESULT hr = E_FAIL;

  if(adapterToTry)
  {
    if(swapDesc)
      hr = dyn_D3D11CreateDeviceAndSwapChain(adapterToTry, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags,
                                             features, 1, D3D11_SDK_VERSION, swapDesc, &swap, &dev,
                                             NULL, &ctx);
    else
      hr = dyn_D3D11CreateDevice(adapterToTry, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, features, 1,
                                 D3D11_SDK_VERSION, &dev, NULL, &ctx);

    if(SUCCEEDED(hr))
      return hr;
  }
  else
  {
    for(size_t i = 0; i < adapters.size(); ++i)
    {
      if(swapDesc)
        hr = dyn_D3D11CreateDeviceAndSwapChain(adapters[i], D3D_DRIVER_TYPE_UNKNOWN, NULL, flags,
                                               features, 1, D3D11_SDK_VERSION, swapDesc, &swap,
                                               &dev, NULL, &ctx);
      else
        hr = dyn_D3D11CreateDevice(adapters[i], D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, features, 1,
                                   D3D11_SDK_VERSION, &dev, NULL, &ctx);

      if(SUCCEEDED(hr))
        break;
    }
  }

  // If it failed, try again on warp
  if(FAILED(hr))
  {
    if(swapDesc)
      hr = dyn_D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, features, 1,
                                             D3D11_SDK_VERSION, swapDesc, &swap, &dev, NULL, &ctx);
    else
      hr = dyn_D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, features, 1,
                                 D3D11_SDK_VERSION, &dev, NULL, &ctx);
  }

  // If it failed again, try last on ref
  if(FAILED(hr))
  {
    if(swapDesc)
      hr = dyn_D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_REFERENCE, NULL, flags, features,
                                             1, D3D11_SDK_VERSION, swapDesc, &swap, &dev, NULL, &ctx);
    else
      hr = dyn_D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_REFERENCE, NULL, flags, features, 1,
                                 D3D11_SDK_VERSION, &dev, NULL, &ctx);
  }

  return hr;
}

void D3D11GraphicsTest::PostDeviceCreate()
{
  {
    IDXGIDevicePtr pDXGIDevice = dev;

    if(!pDXGIDevice)
    {
      TEST_ERROR("Couldn't get DXGI Device");
    }
    else
    {
      IDXGIAdapterPtr pDXGIAdapter;
      HRESULT hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

      if(FAILED(hr) || !pDXGIAdapter)
      {
        TEST_ERROR("Couldn't get DXGI Adapter");
      }
      else
      {
        pDXGIAdapter->GetDesc(&adapterDesc);

        TEST_LOG("Running D3D11 test on %ls", adapterDesc.Description);
      }
    }
  }

  dev1 = dev;
  dev2 = dev;
  dev3 = dev;
  dev4 = dev;
  dev5 = dev;

  ctx1 = ctx;
  ctx2 = ctx;
  ctx3 = ctx;
  ctx4 = ctx;

  fact = factory;

  annot = ctx;

  std::string blitPixel = R"EOSHADER(

Texture2D<float4> tex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target0
{
	return tex.Load(int3(pos.xy, 0));
}

)EOSHADER";

  if(feature_level >= D3D_FEATURE_LEVEL_10_0)
  {
    swapBlitVS = CreateVS(Compile(D3DFullscreenQuadVertex, "main", "vs_4_0"));
    swapBlitPS = CreatePS(Compile(blitPixel, "main", "ps_5_0"));

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    DefaultTriVS = CreateVS(vsblob);
    DefaultTriPS = CreatePS(psblob);

    DefaultTriVB = MakeBuffer().Vertex().Data(DefaultTri);
  }
}

void D3D11GraphicsTest::Shutdown()
{
  delete mainWindow;

  swap = NULL;
  defaultLayout = NULL;

  bbTex = NULL;
  bbRTV = NULL;

  annot = NULL;
  ctx2 = NULL;
  ctx1 = NULL;
  ctx = NULL;

  dev1 = NULL;
  dev2 = NULL;
  dev = NULL;

  swapBlitVS = NULL;
  swapBlitPS = NULL;

  DefaultTriVS = NULL;
  DefaultTriPS = NULL;

  DefaultTriVB = NULL;
}

bool D3D11GraphicsTest::Running()
{
  if(!FrameLimit())
    return false;

  return mainWindow->Update();
}

void D3D11GraphicsTest::Present()
{
  swap->Present(0, 0);
}

void D3D11GraphicsTest::pushMarker(const std::string &name)
{
  if(annot)
    annot->BeginEvent(UTF82Wide(name).c_str());
}

void D3D11GraphicsTest::setMarker(const std::string &name)
{
  if(annot)
    annot->SetMarker(UTF82Wide(name).c_str());
}

void D3D11GraphicsTest::popMarker()
{
  if(annot)
    annot->EndEvent();
}

void D3D11GraphicsTest::blitToSwap(ID3D11Texture2DPtr tex)
{
  ID3D11VertexShaderPtr vs;
  ctx->VSGetShader(&vs, NULL, NULL);
  ID3D11PixelShaderPtr ps;
  ctx->PSGetShader(&ps, NULL, NULL);

  ID3D11ShaderResourceViewPtr srv = NULL;
  ctx->PSGetShaderResources(0, 1, &srv);

  D3D11_PRIMITIVE_TOPOLOGY topo;
  ctx->IAGetPrimitiveTopology(&topo);

  ID3D11InputLayoutPtr layout;
  ctx->IAGetInputLayout(&layout);

  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  ctx->VSSetShader(swapBlitVS, NULL, 0);
  ctx->PSSetShader(swapBlitPS, NULL, 0);

  D3D11_RASTERIZER_DESC oldRS = GetRasterState();
  D3D11_DEPTH_STENCIL_DESC oldDS = GetDepthState();

  ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

  D3D11_RASTERIZER_DESC rs = oldRS;
  rs.CullMode = D3D11_CULL_NONE;
  rs.FillMode = D3D11_FILL_SOLID;
  rs.ScissorEnable = FALSE;
  SetRasterState(rs);

  D3D11_DEPTH_STENCIL_DESC ds = oldDS;
  ds.DepthEnable = FALSE;
  ds.StencilEnable = FALSE;
  SetDepthState(ds);

  ID3D11ShaderResourceViewPtr srcSRV = MakeSRV(tex);
  ctx->PSSetShaderResources(0, 1, &srcSRV.GetInterfacePtr());

  RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

  ctx->IASetInputLayout(NULL);

  ctx->Draw(4, 0);

  ctx->IASetInputLayout(layout);
  ctx->IASetPrimitiveTopology(topo);
  ctx->VSSetShader(vs, NULL, 0);
  ctx->PSSetShader(ps, NULL, 0);
  ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());
  SetRasterState(oldRS);
  SetDepthState(oldDS);
}

std::vector<byte> D3D11GraphicsTest::GetBufferData(ID3D11Buffer *buffer, uint32_t offset, uint32_t len)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  TEST_ASSERT(buffer, "buffer is NULL");

  D3D11_BUFFER_DESC desc;
  buffer->GetDesc(&desc);

  if(len == 0)
    len = desc.ByteWidth - offset;

  if(len > 0 && offset + len > desc.ByteWidth)
  {
    TEST_WARN("Attempting to read off the end of the array. Will be clamped");
    len = std::min(len, desc.ByteWidth - offset);
  }

  ID3D11BufferPtr stage;

  desc.BindFlags = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;
  desc.StructureByteStride = 0;

  CHECK_HR(dev->CreateBuffer(&desc, NULL, &stage));

  std::vector<byte> ret;
  ret.resize(len);

  if(len > 0)
  {
    ctx->CopyResource(stage, buffer);

    CHECK_HR(ctx->Map(stage, 0, D3D11_MAP_READ, 0, &mapped))

    memcpy(&ret[0], mapped.pData, len);

    ctx->Unmap(stage, 0);
  }

  return ret;
}

void D3D11GraphicsTest::IASetVertexBuffer(ID3D11Buffer *vb, UINT stride, UINT offset)
{
  ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
}

void D3D11GraphicsTest::ClearRenderTargetView(ID3D11RenderTargetView *rt, Vec4f col)
{
  ctx->ClearRenderTargetView(rt, &col.x);
}

void D3D11GraphicsTest::RSSetViewport(D3D11_VIEWPORT view)
{
  ctx->RSSetViewports(1, &view);
}

void D3D11GraphicsTest::RSSetScissor(D3D11_RECT scissor)
{
  ctx->RSSetScissorRects(1, &scissor);
}

D3D11_RASTERIZER_DESC D3D11GraphicsTest::GetRasterState()
{
  ID3D11RasterizerState *state = NULL;
  ctx->RSGetState(&state);

  D3D11_RASTERIZER_DESC ret;

  if(state)
  {
    state->GetDesc(&ret);
    return ret;
  }

  ret.FillMode = D3D11_FILL_SOLID;
  ret.CullMode = D3D11_CULL_BACK;
  ret.FrontCounterClockwise = FALSE;
  ret.DepthBias = 0;
  ret.DepthBiasClamp = 0.0f;
  ret.SlopeScaledDepthBias = 0.0f;
  ret.DepthClipEnable = TRUE;
  ret.ScissorEnable = FALSE;
  ret.MultisampleEnable = FALSE;
  ret.AntialiasedLineEnable = FALSE;

  return ret;
}

void D3D11GraphicsTest::SetRasterState(const D3D11_RASTERIZER_DESC &desc)
{
  ID3D11RasterizerState *state = NULL;
  ctx->RSGetState(&state);

  rastState = NULL;
  dev->CreateRasterizerState(&desc, &rastState);

  ctx->RSSetState(rastState);
}

D3D11_DEPTH_STENCIL_DESC D3D11GraphicsTest::GetDepthState()
{
  ID3D11DepthStencilState *state = NULL;
  UINT ref = 0;
  ctx->OMGetDepthStencilState(&state, &ref);

  D3D11_DEPTH_STENCIL_DESC ret;

  if(state)
  {
    state->GetDesc(&ret);
    return ret;
  }

  ret.DepthEnable = TRUE;
  ret.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  ret.DepthFunc = D3D11_COMPARISON_LESS;
  ret.StencilEnable = FALSE;
  ret.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
  ret.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
  const D3D11_DEPTH_STENCILOP_DESC op = {
      D3D11_STENCIL_OP_KEEP,
      D3D11_STENCIL_OP_KEEP,
      D3D11_STENCIL_OP_KEEP,
      D3D11_COMPARISON_ALWAYS,
  };
  ret.FrontFace = op;
  ret.BackFace = op;

  return ret;
}

void D3D11GraphicsTest::SetDepthState(const D3D11_DEPTH_STENCIL_DESC &desc)
{
  ID3D11DepthStencilState *state = NULL;
  UINT ref = 0;
  ctx->OMGetDepthStencilState(&state, &ref);

  depthState = NULL;
  dev->CreateDepthStencilState(&desc, &depthState);

  ctx->OMSetDepthStencilState(depthState, ref);
}

void D3D11GraphicsTest::SetStencilRef(UINT ref)
{
  ID3D11DepthStencilState *state = NULL;
  UINT dummy = 0;
  ctx->OMGetDepthStencilState(&state, &dummy);

  ctx->OMSetDepthStencilState(state, ref);
}

ID3DBlobPtr D3D11GraphicsTest::Compile(std::string src, std::string entry, std::string profile,
                                       bool skipoptimise)
{
  ID3DBlobPtr blob = NULL;
  ID3DBlobPtr error = NULL;

  UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_DEBUG;

  if(skipoptimise)
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_OPTIMIZATION_LEVEL0;
  else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;

  HRESULT hr = dyn_D3DCompile(src.c_str(), src.length(), "", NULL, NULL, entry.c_str(),
                              profile.c_str(), flags, 0, &blob, &error);

  if(FAILED(hr))
  {
    TEST_ERROR("Failed to compile shader, error %x / %s", hr,
               error ? (char *)error->GetBufferPointer() : "Unknown");

    blob = NULL;
    error = NULL;
    return NULL;
  }

  return blob;
}

void D3D11GraphicsTest::Strip(ID3DBlobPtr &ptr)
{
  ID3DBlobPtr stripped = NULL;

  dyn_D3DStripShader(ptr->GetBufferPointer(), ptr->GetBufferSize(),
                     D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO, &stripped);

  ptr = stripped;
}

void D3D11GraphicsTest::WriteBlob(std::string name, ID3DBlobPtr blob, bool compress)
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

void D3D11GraphicsTest::SetBlobPath(std::string name, ID3DBlobPtr &blob)
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

void D3D11GraphicsTest::SetBlobPath(std::string name, ID3D11DeviceChild *shader)
{
  const GUID RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

  shader->SetPrivateData(RENDERDOC_ShaderDebugMagicValue, (UINT)name.size() + 1, name.c_str());
}

void D3D11GraphicsTest::CreateDefaultInputLayout(ID3DBlobPtr vsblob)
{
  D3D11_INPUT_ELEMENT_DESC layoutdesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                  vsblob->GetBufferSize(), &defaultLayout));
}

ID3D11VertexShaderPtr D3D11GraphicsTest::CreateVS(ID3DBlobPtr blob)
{
  ID3D11VertexShaderPtr ret;
  CHECK_HR(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &ret));
  return ret;
}

ID3D11PixelShaderPtr D3D11GraphicsTest::CreatePS(ID3DBlobPtr blob)
{
  ID3D11PixelShaderPtr ret;
  CHECK_HR(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &ret));
  return ret;
}

ID3D11ComputeShaderPtr D3D11GraphicsTest::CreateCS(ID3DBlobPtr blob)
{
  ID3D11ComputeShaderPtr ret;
  CHECK_HR(dev->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &ret));
  return ret;
}

ID3D11GeometryShaderPtr D3D11GraphicsTest::CreateGS(
    ID3DBlobPtr blob, const std::vector<D3D11_SO_DECLARATION_ENTRY> &sodecl,
    const std::vector<UINT> &strides, UINT rastStream)
{
  ID3D11GeometryShaderPtr ret;
  CHECK_HR(dev->CreateGeometryShaderWithStreamOutput(blob->GetBufferPointer(), blob->GetBufferSize(),
                                                     &sodecl[0], (UINT)sodecl.size(), &strides[0],
                                                     (UINT)strides.size(), rastStream, NULL, &ret));
  return ret;
}

ID3D11GeometryShaderPtr D3D11GraphicsTest::CreateGS(ID3DBlobPtr blob)
{
  ID3D11GeometryShaderPtr ret;
  CHECK_HR(dev->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &ret));
  return ret;
}
