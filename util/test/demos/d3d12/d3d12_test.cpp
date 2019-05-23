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

#define INITGUID

#include "d3d12_test.h"
#include <stdio.h>
#include "../3rdparty/lz4/lz4.h"
#include "../renderdoc_app.h"
#include "../win32/win32_window.h"

typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY2)(UINT, REFIID, void **);

namespace
{
HMODULE d3d12 = NULL;
HMODULE dxgi = NULL;
HMODULE d3dcompiler = NULL;
IDXGIFactory4Ptr factory;
IDXGIAdapterPtr adapter;
};

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

    if(d3d12)
    {
      PFN_CREATE_DXGI_FACTORY2 createFactory2 =
          (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgi, "CreateDXGIFactory2");

      if(createFactory2)
      {
        HRESULT hr = createFactory2(debugDevice ? DXGI_CREATE_FACTORY_DEBUG : 0,
                                    __uuidof(IDXGIFactory4), (void **)&factory);

        if(SUCCEEDED(hr))
        {
          bool warp = false;

          adapter = ChooseD3DAdapter(factory, argc, argv, warp);

          if(warp)
            factory->EnumWarpAdapter(__uuidof(IDXGIAdapter), (void **)&adapter);
        }
      }
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

  m_Factory = factory;
}

bool D3D12GraphicsTest::Init()
{
  // parse parameters here to override parameters
  if(!GraphicsTest::Init())
    return false;

  // we can assume d3d12, dxgi and d3dcompiler are valid since we shouldn't be running the test if
  // Prepare() failed

  dyn_D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice");

  dyn_D3DCompile = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");
  dyn_D3DStripShader = (pD3DStripShader)GetProcAddress(d3dcompiler, "D3DStripShader");
  dyn_D3DSetBlobPart = (pD3DSetBlobPart)GetProcAddress(d3dcompiler, "D3DSetBlobPart");

  dyn_serializeRootSig = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(
      d3d12, "D3D12SerializeVersionedRootSignature");
  dyn_serializeRootSigOld =
      (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(d3d12, "D3D12SerializeRootSignature");

  if(dyn_serializeRootSig == NULL)
  {
    TEST_WARN("Can't get D3D12SerializeVersionedRootSignature - old version of windows?");

    if(dyn_serializeRootSigOld == NULL)
    {
      TEST_ERROR("Can't get D3D12SerializeRootSignature!");
      return false;
    }
  }

  if(debugDevice)
  {
    PFN_D3D12_GET_DEBUG_INTERFACE getD3D12DebugInterface =
        (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(d3d12, "D3D12GetDebugInterface");

    if(!getD3D12DebugInterface)
    {
      TEST_ERROR("Couldn't find D3D12GetDebugInterface!");
      return false;
    }

    HRESULT hr = getD3D12DebugInterface(__uuidof(ID3D12Debug), (void **)&d3d12Debug);

    if(SUCCEEDED(hr) && d3d12Debug)
    {
      d3d12Debug->EnableDebugLayer();
    }
  }

  {
    HRESULT hr = S_OK;

    hr = dyn_D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                               (void **)&dev);

    if(FAILED(hr))
    {
      TEST_ERROR("D3D12CreateDevice failed: %x", hr);
      return false;
    }
  }

  {
    LUID luid = dev->GetAdapterLuid();

    IDXGIAdapterPtr pDXGIAdapter;
    HRESULT hr = m_Factory->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

    if(FAILED(hr))
    {
      TEST_ERROR("Couldn't get DXGI adapter by LUID from D3D device");
    }
    else
    {
      DXGI_ADAPTER_DESC desc = {};
      pDXGIAdapter->GetDesc(&desc);

      TEST_LOG("Running D3D12 test on %ls", desc.Description);
    }
  }

  {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    dev->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void **)&queue);
  }

  if(!headless)
  {
    Win32Window *win = new Win32Window(screenWidth, screenHeight, screenTitle);

    mainWindow = win;

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

    CHECK_HR(m_Factory->CreateSwapChainForHwnd(queue, win->wnd, &swapDesc, NULL, NULL, &swap));

    CHECK_HR(swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&bbTex[0]));
    CHECK_HR(swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&bbTex[1]));
  }

  dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_GPUSyncFence);
  m_GPUSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  m_GPUSyncFence->SetName(L"GPUSync fence");

  const D3D12_INPUT_CLASSIFICATION vertex = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

  m_DefaultInputLayout = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, vertex, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, vertex, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, vertex, 0},
  };

  CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       __uuidof(ID3D12CommandAllocator), (void **)&m_Alloc));

  m_Alloc->SetName(L"Command allocator");

  CHECK_HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Alloc, NULL,
                                  __uuidof(ID3D12GraphicsCommandList), (void **)&m_DebugList));

  // command buffers are allocated opened, close it immediately.
  m_DebugList->Close();

  m_DebugList->SetName(L"Debug command list");

  {
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 1;
    desc.NumDescriptors = 8;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_RTV));

    m_RTV->SetName(L"RTV heap");

    desc.NumDescriptors = 1;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_DSV));

    m_DSV->SetName(L"DSV heap");

    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    desc.NumDescriptors = 8;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_Sampler));

    m_Sampler->SetName(L"Sampler heap");

    desc.NumDescriptors = 1024;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_CBVUAVSRV));

    m_CBVUAVSRV->SetName(L"CBV/UAV/SRV heap");
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

  // mute useless messages
  D3D12_MESSAGE_ID mute[] = {
      // super spammy, mostly just perf warning
      D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
      D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
  };

  dev->QueryInterface(__uuidof(ID3D12InfoQueue), (void **)&infoqueue);

  if(infoqueue)
  {
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = ARRAY_COUNT(mute);
    filter.DenyList.pIDList = mute;

    infoqueue->AddStorageFilterEntries(&filter);
  }

  return true;
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

  ResourceBarrier(cmd, bbTex[texIdx], D3D12_RESOURCE_STATE_PRESENT, useState);

  return bbTex[texIdx];
}

void D3D12GraphicsTest::FinishUsingBackbuffer(ID3D12GraphicsCommandListPtr cmd,
                                              D3D12_RESOURCE_STATES usedState)
{
  ID3D12ResourcePtr bb = bbTex[texIdx];

  ResourceBarrier(cmd, bbTex[texIdx], usedState, D3D12_RESOURCE_STATE_PRESENT);

  texIdx = 1 - texIdx;
}

void D3D12GraphicsTest::Submit(const std::vector<ID3D12GraphicsCommandListPtr> &cmds)
{
  std::vector<ID3D12CommandList *> submits;

  m_GPUSyncCounter++;

  for(const ID3D12GraphicsCommandListPtr &cmd : cmds)
  {
    pendingCommandBuffers.push_back(std::make_pair(cmd, m_GPUSyncFence));
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

void D3D12GraphicsTest::Present()
{
  swap->Present(0, 0);

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

void D3D12GraphicsTest::RSSetViewport(ID3D12GraphicsCommandListPtr cmd, D3D12_VIEWPORT view)
{
  cmd->RSSetViewports(1, &view);
}

void D3D12GraphicsTest::RSSetScissorRect(ID3D12GraphicsCommandListPtr cmd, D3D12_RECT rect)
{
  cmd->RSSetScissorRects(1, &rect);
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

ID3DBlobPtr D3D12GraphicsTest::Compile(std::string src, std::string entry, std::string profile,
                                       ID3DBlob **unstripped)
{
  ID3DBlobPtr blob = NULL;
  ID3DBlobPtr error = NULL;

  HRESULT hr =
      dyn_D3DCompile(src.c_str(), src.length(), "", NULL, NULL, entry.c_str(), profile.c_str(),
                     D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_DEBUG |
                         D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_OPTIMIZATION_LEVEL0,
                     0, &blob, &error);

  if(FAILED(hr))
  {
    TEST_ERROR("Failed to compile shader, error %x / %s", hr,
               error ? (char *)error->GetBufferPointer() : "Unknown");

    blob = NULL;
    error = NULL;
    return NULL;
  }

  if(unstripped)
  {
    blob.AddRef();
    *unstripped = blob.GetInterfacePtr();

    ID3DBlobPtr stripped = NULL;

    dyn_D3DStripShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                       D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO, &stripped);

    blob = NULL;

    return stripped;
  }

  return blob;
}

void D3D12GraphicsTest::WriteBlob(std::string name, ID3DBlob *blob, bool compress)
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

ID3DBlobPtr D3D12GraphicsTest::SetBlobPath(std::string name, ID3DBlob *blob)
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

  return newBlob;
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
    CHECK_HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Alloc, NULL,
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
    HRESULT hr = dyn_serializeRootSig(&verdesc, &blob, &errBlob);

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