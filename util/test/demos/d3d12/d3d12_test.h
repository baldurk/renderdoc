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

#include <set>
#include "dx/official/dxgi.h"
#include "d3d12_helpers.h"

#include "dx/official/d3dcompiler.h"

typedef HRESULT(WINAPI *pD3DStripShader)(_In_reads_bytes_(BytecodeLength) LPCVOID pShaderBytecode,
                                         _In_ SIZE_T BytecodeLength, _In_ UINT uStripFlags,
                                         _Out_ ID3DBlob **ppStrippedBlob);
typedef HRESULT(WINAPI *pD3DSetBlobPart)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                                         _In_ SIZE_T SrcDataSize, _In_ D3D_BLOB_PART Part,
                                         _In_ UINT Flags, _In_reads_bytes_(PartSize) LPCVOID pPart,
                                         _In_ SIZE_T PartSize, _Out_ ID3DBlob **ppNewShader);
typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);

struct Win32Window;

struct D3D12GraphicsTest : public GraphicsTest
{
  static const TestAPI API = TestAPI::D3D12;

  void Prepare(int argc, char **argv);
  bool Init();

  void PostDeviceCreate();

  void Shutdown();
  GraphicsWindow *MakeWindow(int width, int height, const char *title);

  DXGI_SWAP_CHAIN_DESC1 MakeSwapchainDesc();
  std::vector<IDXGIAdapterPtr> GetAdapters();
  HRESULT EnumAdapterByLuid(LUID luid, IDXGIAdapterPtr &pAdapter);
  ID3D12DevicePtr CreateDevice(const std::vector<IDXGIAdapterPtr> &adaptersToTry,
                               D3D_FEATURE_LEVEL features);

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
  void WriteBlob(std::string name, ID3DBlobPtr blob, bool compress);

  void SetBlobPath(std::string name, ID3DBlobPtr &blob);
  void SetBlobPath(std::string name, ID3D12DeviceChild *shader);

  ID3D12GraphicsCommandListPtr GetCommandBuffer();

  void Reset(ID3D12GraphicsCommandListPtr cmd);

  ID3D12RootSignaturePtr MakeSig(
      const std::vector<D3D12_ROOT_PARAMETER1> &params,
      D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
      UINT NumStaticSamplers = 0, const D3D12_STATIC_SAMPLER_DESC *StaticSamplers = NULL);
  ID3D12CommandSignaturePtr MakeCommandSig(ID3D12RootSignaturePtr rootSig,
                                           const std::vector<D3D12_INDIRECT_ARGUMENT_DESC> &params);
  D3D12PSOCreator MakePSO() { return D3D12PSOCreator(dev); }
  D3D12BufferCreator MakeBuffer() { return D3D12BufferCreator(dev, this); }
  D3D12TextureCreator MakeTexture(DXGI_FORMAT format, UINT width)
  {
    return D3D12TextureCreator(dev, format, width, 1, 1);
  }
  D3D12TextureCreator MakeTexture(DXGI_FORMAT format, UINT width, UINT height)
  {
    return D3D12TextureCreator(dev, format, width, height, 1);
  }
  D3D12TextureCreator MakeTexture(DXGI_FORMAT format, UINT width, UINT height, UINT depth)
  {
    return D3D12TextureCreator(dev, format, width, height, depth);
  }

  template <typename T>
  D3D12ViewCreator MakeCBV(T res)
  {
    return D3D12ViewCreator(dev, m_CBVUAVSRV, NULL, ViewType::CBV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeSRV(T res)
  {
    return D3D12ViewCreator(dev, m_CBVUAVSRV, NULL, ViewType::SRV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeRTV(T res)
  {
    return D3D12ViewCreator(dev, m_RTV, NULL, ViewType::RTV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeDSV(T res)
  {
    return D3D12ViewCreator(dev, m_DSV, NULL, ViewType::DSV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeUAV(T res)
  {
    return D3D12ViewCreator(dev, m_CBVUAVSRV, m_Clear, ViewType::UAV, res);
  }

  std::vector<byte> GetBufferData(ID3D12ResourcePtr buffer, D3D12_RESOURCE_STATES state,
                                  uint32_t offset = 0, uint64_t length = 0);
  void SetBufferData(ID3D12ResourcePtr buffer, D3D12_RESOURCE_STATES state, const byte *data,
                     uint64_t len);

  byte *Map(ID3D12ResourcePtr res, UINT sub)
  {
    byte *ret = NULL;
    res->Map(sub, NULL, (void **)&ret);
    return ret;
  }

  void pushMarker(ID3D12GraphicsCommandListPtr cmd, const std::string &name);
  void setMarker(ID3D12GraphicsCommandListPtr cmd, const std::string &name);
  void popMarker(ID3D12GraphicsCommandListPtr cmd);

  void blitToSwap(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr src, ID3D12ResourcePtr dst,
                  DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN);

  void ResourceBarrier(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr res,
                       D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
  void ResourceBarrier(ID3D12ResourcePtr res, D3D12_RESOURCE_STATES before,
                       D3D12_RESOURCE_STATES after);

  void IASetVertexBuffer(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr vb, UINT stride,
                         UINT offset);
  void IASetIndexBuffer(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr ib, DXGI_FORMAT fmt,
                        UINT offset);

  void ClearRenderTargetView(ID3D12GraphicsCommandListPtr cmd, D3D12_CPU_DESCRIPTOR_HANDLE rt,
                             Vec4f col);
  void ClearRenderTargetView(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr rt, Vec4f col);
  void ClearDepthStencilView(ID3D12GraphicsCommandListPtr cmd, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                             D3D12_CLEAR_FLAGS flags, float depth, UINT8 stencil);
  void ClearDepthStencilView(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr dsv,
                             D3D12_CLEAR_FLAGS flags, float depth, UINT8 stencil);

  void RSSetViewport(ID3D12GraphicsCommandListPtr cmd, D3D12_VIEWPORT view);
  void RSSetScissorRect(ID3D12GraphicsCommandListPtr cmd, D3D12_RECT rect);

  void SetMainWindowViewScissor(ID3D12GraphicsCommandListPtr cmd);

  void OMSetRenderTargets(ID3D12GraphicsCommandListPtr cmd,
                          const std::vector<ID3D12ResourcePtr> &rtvs, ID3D12ResourcePtr dsv);
  void OMSetRenderTargets(ID3D12GraphicsCommandListPtr cmd,
                          const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> &rtvs,
                          D3D12_CPU_DESCRIPTOR_HANDLE dsv);

  bool Running();
  ID3D12ResourcePtr StartUsingBackbuffer(ID3D12GraphicsCommandListPtr cmd,
                                         D3D12_RESOURCE_STATES useState);
  void FinishUsingBackbuffer(ID3D12GraphicsCommandListPtr cmd, D3D12_RESOURCE_STATES usedState);
  void Submit(const std::vector<ID3D12GraphicsCommandListPtr> &cmds);
  void GPUSync();
  void SubmitAndPresent(const std::vector<ID3D12GraphicsCommandListPtr> &cmds);
  void Present();

  DXGI_FORMAT backbufferFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
  int backbufferCount = 2;

  GraphicsWindow *mainWindow = NULL;

  DXGI_ADAPTER_DESC adapterDesc = {};

  IDXGISwapChain1Ptr swap;

  ID3D12ResourcePtr bbTex[2];
  uint32_t texIdx = 0;
  D3D12_CPU_DESCRIPTOR_HANDLE BBRTV;

  ID3D12RootSignaturePtr swapBlitSig;
  ID3D12PipelineStatePtr swapBlitPso;

  ID3D12ResourcePtr DefaultTriVB;
  ID3D12RootSignaturePtr DefaultTriSig;
  ID3D12PipelineStatePtr DefaultTriPSO;

  D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  std::string d3d12path;

  bool gpuva = false, m_12On7 = false, m_DXILSupport = false;
  IDXGIFactory1Ptr m_Factory;

  ID3D12DebugPtr d3d12Debug;
  ID3D12InfoQueuePtr infoqueue;

  ID3D12DeviceFactoryPtr devFactory;
  ID3D12DeviceConfigurationPtr devConfig;

  ID3D12DevicePtr dev;
  ID3D12Device1Ptr dev1;
  ID3D12Device2Ptr dev2;
  ID3D12Device3Ptr dev3;
  ID3D12Device4Ptr dev4;
  ID3D12Device5Ptr dev5;
  ID3D12Device6Ptr dev6;
  ID3D12Device7Ptr dev7;
  ID3D12Device8Ptr dev8;

  ID3D12DescriptorHeapPtr m_RTV, m_DSV, m_CBVUAVSRV, m_Clear, m_Sampler;

  ID3D12CommandAllocatorPtr m_Alloc;
  ID3D12GraphicsCommandListPtr m_DebugList;

  D3D12_COMMAND_LIST_TYPE queueType = D3D12_COMMAND_LIST_TYPE_DIRECT;
  ID3D12CommandQueuePtr queue;

  D3D12_FEATURE_DATA_D3D12_OPTIONS opts = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 opts1 = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS2 opts2 = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS3 opts3 = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS4 opts4 = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS6 opts6 = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS7 opts7 = {};
  D3D_SHADER_MODEL m_HighestShaderModel = D3D_SHADER_MODEL_5_1;

  ID3D12FencePtr m_GPUSyncFence;
  HANDLE m_GPUSyncHandle = NULL;
  UINT64 m_GPUSyncCounter = 1;

  static const uint64_t m_DebugBufferSize = 64 * 1024 * 1024;
  ID3D12ResourcePtr m_ReadbackBuffer, m_UploadBuffer;

  std::vector<ID3D12GraphicsCommandListPtr> freeCommandBuffers;
  std::vector<std::pair<ID3D12GraphicsCommandListPtr, UINT64>> pendingCommandBuffers;

private:
  void AddHashIfMissing(void *ByteCode, size_t BytecodeLength);
};
