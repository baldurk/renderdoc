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

#pragma once

#include "../test_common.h"

#include "dx/official/dxgi.h"
#include "d3d11_helpers.h"

#include "dx/official/d3dcompiler.h"

typedef HRESULT(WINAPI *pD3DStripShader)(_In_reads_bytes_(BytecodeLength) LPCVOID pShaderBytecode,
                                         _In_ SIZE_T BytecodeLength, _In_ UINT uStripFlags,
                                         _Out_ ID3DBlob **ppStrippedBlob);
typedef HRESULT(WINAPI *pD3DSetBlobPart)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                                         _In_ SIZE_T SrcDataSize, _In_ D3D_BLOB_PART Part,
                                         _In_ UINT Flags, _In_reads_bytes_(PartSize) LPCVOID pPart,
                                         _In_ SIZE_T PartSize, _Out_ ID3DBlob **ppNewShader);

struct Win32Window;

struct D3D11GraphicsTest : public GraphicsTest
{
  static const TestAPI API = TestAPI::D3D11;

  void Prepare(int argc, char **argv);
  bool Init(IDXGIAdapterPtr pAdapter = NULL);

  void Shutdown();
  GraphicsWindow *MakeWindow(int width, int height, const char *title);
  DXGI_SWAP_CHAIN_DESC MakeSwapchainDesc(GraphicsWindow *win);

  std::vector<IDXGIAdapterPtr> GetAdapters();

  HRESULT CreateDevice(IDXGIAdapterPtr adapterToTry, DXGI_SWAP_CHAIN_DESC *swapDesc,
                       D3D_FEATURE_LEVEL *features, UINT flags);
  void PostDeviceCreate();

  enum BufType
  {
    eCBuffer = 0x0,
    eStageBuffer = 0x1,
    eVBuffer = 0x2,
    eIBuffer = 0x4,
    eBuffer = 0x8,
    eCompBuffer = 0x10,
    eSOBuffer = 0x20,
    BufMajorType = 0xff,

    eAppend = 0x100,
    eRawBuffer = 0x200,
    BufUAVType = 0xf00,
  };

  ID3DBlobPtr Compile(std::string src, std::string entry, std::string profile,
                      bool skipoptimise = true);
  void Strip(ID3DBlobPtr &ptr);
  void WriteBlob(std::string name, ID3DBlobPtr blob, bool compress);

  ID3D11VertexShaderPtr CreateVS(ID3DBlobPtr blob);
  ID3D11PixelShaderPtr CreatePS(ID3DBlobPtr blob);
  ID3D11ComputeShaderPtr CreateCS(ID3DBlobPtr blob);
  ID3D11GeometryShaderPtr CreateGS(ID3DBlobPtr blob);
  ID3D11GeometryShaderPtr CreateGS(ID3DBlobPtr blob,
                                   const std::vector<D3D11_SO_DECLARATION_ENTRY> &sodecl,
                                   const std::vector<UINT> &strides, UINT rastStream = 0);

  void SetBlobPath(std::string name, ID3DBlobPtr &blob);
  void SetBlobPath(std::string name, ID3D11DeviceChild *shader);

  void CreateDefaultInputLayout(ID3DBlobPtr vsblob);

  D3D11BufferCreator MakeBuffer() { return D3D11BufferCreator(dev); }
  D3D11TextureCreator MakeTexture(DXGI_FORMAT format, UINT width)
  {
    return D3D11TextureCreator(dev, format, width, 1, 1);
  }
  D3D11TextureCreator MakeTexture(DXGI_FORMAT format, UINT width, UINT height)
  {
    return D3D11TextureCreator(dev, format, width, height, 1);
  }
  D3D11TextureCreator MakeTexture(DXGI_FORMAT format, UINT width, UINT height, UINT depth)
  {
    return D3D11TextureCreator(dev, format, width, height, depth);
  }

  D3D11SamplerCreator MakeSampler() { return D3D11SamplerCreator(dev); }
  template <typename T>
  D3D11ViewCreator MakeSRV(T res)
  {
    return D3D11ViewCreator(dev, ViewType::SRV, res);
  }
  template <typename T>
  D3D11ViewCreator MakeRTV(T res)
  {
    return D3D11ViewCreator(dev, ViewType::RTV, res);
  }
  template <typename T>
  D3D11ViewCreator MakeDSV(T res)
  {
    return D3D11ViewCreator(dev, ViewType::DSV, res);
  }
  template <typename T>
  D3D11ViewCreator MakeUAV(T res)
  {
    return D3D11ViewCreator(dev, ViewType::UAV, res);
  }

  std::vector<byte> GetBufferData(ID3D11Buffer *buf, uint32_t offset = 0, uint32_t len = 0);

  D3D11_MAPPED_SUBRESOURCE Map(ID3D11Resource *res, UINT sub, D3D11_MAP type)
  {
    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(res, sub, type, 0, &mapped);
    return mapped;
  }

  struct VBBind
  {
    ID3D11Buffer *buf;
    UINT stride;
    UINT offset;
  };

  void IASetVertexBuffer(ID3D11Buffer *vb, UINT stride, UINT offset);

  void ClearRenderTargetView(ID3D11RenderTargetView *rt, Vec4f col);

  D3D11_RASTERIZER_DESC GetRasterState();
  void SetRasterState(const D3D11_RASTERIZER_DESC &desc);
  void RSSetViewport(D3D11_VIEWPORT view);
  void RSSetScissor(D3D11_RECT scissor);

  D3D11_DEPTH_STENCIL_DESC GetDepthState();
  void SetDepthState(const D3D11_DEPTH_STENCIL_DESC &desc);
  void SetStencilRef(UINT ref);

  bool Running();
  void Present();

  void pushMarker(const std::string &name);
  void setMarker(const std::string &name);
  void popMarker();

  void blitToSwap(ID3D11Texture2DPtr tex);

  DXGI_FORMAT backbufferFmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  int backbufferCount = 2;
  int backbufferMSAA = 1;
  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  UINT createFlags = 0;

  DXGI_ADAPTER_DESC adapterDesc = {};

  ID3D11VertexShaderPtr DefaultTriVS;
  ID3D11PixelShaderPtr DefaultTriPS;

  ID3D11BufferPtr DefaultTriVB;

  ID3D11VertexShaderPtr swapBlitVS;
  ID3D11PixelShaderPtr swapBlitPS;

  D3D11_FEATURE_DATA_D3D11_OPTIONS opts = {};
  D3D11_FEATURE_DATA_D3D11_OPTIONS1 opts1 = {};
  D3D11_FEATURE_DATA_D3D11_OPTIONS2 opts2 = {};

  ID3D11DepthStencilStatePtr depthState;
  ID3D11RasterizerStatePtr rastState;

  GraphicsWindow *mainWindow = NULL;

  IDXGIFactory1Ptr fact;
  IDXGISwapChainPtr swap;

  ID3D11InputLayoutPtr defaultLayout;

  ID3D11Texture2DPtr bbTex;
  ID3D11RenderTargetViewPtr bbRTV;

  ID3D11DevicePtr dev;
  ID3D11Device1Ptr dev1;
  ID3D11Device2Ptr dev2;
  ID3D11Device3Ptr dev3;
  ID3D11Device4Ptr dev4;
  ID3D11Device5Ptr dev5;

  ID3D11DeviceContextPtr ctx;
  ID3D11DeviceContext1Ptr ctx1;
  ID3D11DeviceContext2Ptr ctx2;
  ID3D11DeviceContext3Ptr ctx3;
  ID3D11DeviceContext4Ptr ctx4;
  ID3DUserDefinedAnnotationPtr annot;

private:
  static bool prepared_d3d11;
};
