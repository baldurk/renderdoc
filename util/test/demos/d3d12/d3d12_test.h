/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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

struct Win32Window;

struct D3D12GraphicsTest : public GraphicsTest
{
  static const TestAPI API = TestAPI::D3D12;

  void Prepare(int argc, char **argv);
  bool Init();
  void Shutdown();
  GraphicsWindow *MakeWindow(int width, int height, const char *title);

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
                      ID3DBlob **unstripped = NULL);
  void WriteBlob(std::string name, ID3DBlob *blob, bool compress);

  const std::vector<D3D12_INPUT_ELEMENT_DESC> &DefaultInputLayout() { return m_DefaultInputLayout; }
  ID3DBlobPtr SetBlobPath(std::string name, ID3DBlob *blob);
  void SetBlobPath(std::string name, ID3D12DeviceChild *shader);

  ID3D12GraphicsCommandListPtr GetCommandBuffer();

  void Reset(ID3D12GraphicsCommandListPtr cmd);

  ID3D12RootSignaturePtr MakeSig(
      const std::vector<D3D12_ROOT_PARAMETER1> &params,
      D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
      UINT NumStaticSamplers = 0, const D3D12_STATIC_SAMPLER_DESC *StaticSamplers = NULL);
  D3D12PSOCreator MakePSO() { return D3D12PSOCreator(this); }
  D3D12BufferCreator MakeBuffer() { return D3D12BufferCreator(this); }
  D3D12TextureCreator MakeTexture(DXGI_FORMAT format, UINT width)
  {
    return D3D12TextureCreator(this, format, width, 1, 1);
  }
  D3D12TextureCreator MakeTexture(DXGI_FORMAT format, UINT width, UINT height)
  {
    return D3D12TextureCreator(this, format, width, height, 1);
  }
  D3D12TextureCreator MakeTexture(DXGI_FORMAT format, UINT width, UINT height, UINT depth)
  {
    return D3D12TextureCreator(this, format, width, height, depth);
  }

  template <typename T>
  D3D12ViewCreator MakeSRV(T res)
  {
    return D3D12ViewCreator(this, m_CBVUAVSRV, ViewType::SRV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeRTV(T res)
  {
    return D3D12ViewCreator(this, m_RTV, ViewType::RTV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeDSV(T res)
  {
    return D3D12ViewCreator(this, m_DSV, ViewType::DSV, res);
  }
  template <typename T>
  D3D12ViewCreator MakeUAV(T res)
  {
    return D3D12ViewCreator(this, m_CBVUAVSRV, ViewType::UAV, res);
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

  void ResourceBarrier(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr res,
                       D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
  void ResourceBarrier(ID3D12ResourcePtr res, D3D12_RESOURCE_STATES before,
                       D3D12_RESOURCE_STATES after);

  void IASetVertexBuffer(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr vb, UINT stride,
                         UINT offset);

  void ClearRenderTargetView(ID3D12GraphicsCommandListPtr cmd, D3D12_CPU_DESCRIPTOR_HANDLE rt,
                             Vec4f col);
  void ClearRenderTargetView(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr rt, Vec4f col);
  void ClearDepthStencilView(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr dsv,
                             D3D12_CLEAR_FLAGS flags, float depth, UINT8 stencil);

  void RSSetViewport(ID3D12GraphicsCommandListPtr cmd, D3D12_VIEWPORT view);
  void RSSetScissorRect(ID3D12GraphicsCommandListPtr cmd, D3D12_RECT rect);

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
  void Present();

  DXGI_FORMAT backbufferFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
  int backbufferCount = 2;

  pD3DCompile dyn_D3DCompile = NULL;
  pD3DStripShader dyn_D3DStripShader = NULL;
  pD3DSetBlobPart dyn_D3DSetBlobPart = NULL;

  PFN_D3D12_CREATE_DEVICE dyn_D3D12CreateDevice = NULL;

  PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE dyn_serializeRootSig;
  PFN_D3D12_SERIALIZE_ROOT_SIGNATURE dyn_serializeRootSigOld;

  GraphicsWindow *mainWindow = NULL;

  IDXGISwapChain1Ptr swap;

  ID3D12ResourcePtr bbTex[2];
  uint32_t texIdx = 0;

  IDXGIFactory4Ptr m_Factory;

  ID3D12DebugPtr d3d12Debug;
  ID3D12InfoQueuePtr infoqueue;

  ID3D12DevicePtr dev;

  ID3D12DescriptorHeapPtr m_RTV, m_DSV, m_CBVUAVSRV, m_Sampler;

  ID3D12CommandAllocatorPtr m_Alloc;
  ID3D12GraphicsCommandListPtr m_DebugList;

  ID3D12CommandQueuePtr queue;

  std::vector<D3D12_INPUT_ELEMENT_DESC> m_DefaultInputLayout;

  ID3D12FencePtr m_GPUSyncFence;
  HANDLE m_GPUSyncHandle = NULL;
  UINT64 m_GPUSyncCounter = 1;

  static const uint64_t m_DebugBufferSize = 64 * 1024 * 1024;
  ID3D12ResourcePtr m_ReadbackBuffer, m_UploadBuffer;

  std::vector<ID3D12GraphicsCommandListPtr> freeCommandBuffers;
  std::vector<std::pair<ID3D12GraphicsCommandListPtr, UINT64>> pendingCommandBuffers;
};