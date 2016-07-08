/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "d3d12_debug.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"

D3D12DebugManager::D3D12DebugManager(WrappedID3D12Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D12DebugManager));

  m_Device = wrapper->GetReal();
  m_ResourceManager = wrapper->GetResourceManager();

  m_OutputWindowID = 1;

  m_WrappedDevice = wrapper;
  m_WrappedDevice->InternalRef();

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.0f);

  m_pFactory = NULL;

  HRESULT hr = S_OK;

  IDXGIDevice *pDXGIDevice;
  hr = m_WrappedDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);

  if(FAILED(hr))
  {
    RDCERR("Couldn't get DXGI device from D3D device");
  }
  else
  {
    IDXGIAdapter *pDXGIAdapter;
    hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

    if(FAILED(hr))
    {
      RDCERR("Couldn't get DXGI adapter from DXGI device");
      SAFE_RELEASE(pDXGIDevice);
    }
    else
    {
      hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&m_pFactory);

      SAFE_RELEASE(pDXGIDevice);
      SAFE_RELEASE(pDXGIAdapter);

      if(FAILED(hr))
      {
        RDCERR("Couldn't get DXGI factory from DXGI adapter");
      }
    }
  }

  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 1;
  desc.NumDescriptors = 1024;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&rtvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create RTV descriptor heap!");
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&dsvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DSV descriptor heap!");
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 1.0f);
}

D3D12DebugManager::~D3D12DebugManager()
{
  SAFE_RELEASE(m_pFactory);

  m_WrappedDevice->InternalRelease();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void D3D12DebugManager::OutputWindow::MakeDSV()
{
  SAFE_RELEASE(depth);

  D3D12_RESOURCE_DESC texDesc = bb->GetDesc();

  texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = dev->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, &texDesc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ, NULL,
      __uuidof(ID3D12Resource), (void **)&depth);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV texture for main output, HRESULT: 0x%08x", hr);
    return;
  }

  dev->CreateDepthStencilView(depth, NULL, dsv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV for main output, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb);
    return;
  }
}

uint64_t D3D12DebugManager::MakeOutputWindow(void *w, bool depth)
{
  OutputWindow outw;
  outw.wnd = (HWND)w;
  outw.dev = m_WrappedDevice;

  DXGI_SWAP_CHAIN_DESC swapDesc;
  RDCEraseEl(swapDesc);

  RECT rect;
  GetClientRect(outw.wnd, &rect);

  swapDesc.BufferCount = 2;
  swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  outw.width = swapDesc.BufferDesc.Width = rect.right - rect.left;
  outw.height = swapDesc.BufferDesc.Height = rect.bottom - rect.top;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.SampleDesc.Count = depth ? 4 : 1;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.OutputWindow = outw.wnd;
  swapDesc.Windowed = TRUE;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  swapDesc.Flags = 0;

  HRESULT hr = S_OK;

  hr = m_pFactory->CreateSwapChain(m_WrappedDevice->GetQueue()->GetReal(), &swapDesc, &outw.swap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create swap chain for HWND, HRESULT: 0x%08x", hr);
    return 0;
  }

  outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb);

  outw.rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  outw.rtv.ptr += m_OutputWindowID *
                  m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  outw.dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
  outw.dsv.ptr += m_OutputWindowID *
                  m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  m_WrappedDevice->CreateRenderTargetView(outw.bb, NULL, outw.rtv);

  outw.depth = NULL;
  if(depth)
    outw.MakeDSV();

  uint64_t id = m_OutputWindowID++;
  m_OutputWindows[id] = outw;
  return id;
}

void D3D12DebugManager::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  SAFE_RELEASE(outw.swap);
  SAFE_RELEASE(outw.bb);
  SAFE_RELEASE(outw.depth);

  m_OutputWindows.erase(it);
}

bool D3D12DebugManager::CheckResizeOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.wnd == NULL || outw.swap == NULL)
    return false;

  RECT rect;
  GetClientRect(outw.wnd, &rect);
  long w = rect.right - rect.left;
  long h = rect.bottom - rect.top;

  if(w != outw.width || h != outw.height)
  {
    outw.width = w;
    outw.height = h;

    m_WrappedDevice->GPUSync();

    if(outw.width > 0 && outw.height > 0)
    {
      SAFE_RELEASE(outw.bb);

      DXGI_SWAP_CHAIN_DESC desc;
      outw.swap->GetDesc(&desc);

      HRESULT hr = outw.swap->ResizeBuffers(desc.BufferCount, outw.width, outw.height,
                                            desc.BufferDesc.Format, desc.Flags);

      if(FAILED(hr))
      {
        RDCERR("Failed to resize swap chain, HRESULT: 0x%08x", hr);
        return true;
      }

      outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb);

      m_WrappedDevice->CreateRenderTargetView(outw.bb, NULL, outw.rtv);
      if(outw.depth)
        outw.MakeDSV();
    }

    return true;
  }

  return false;
}

void D3D12DebugManager::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  w = m_OutputWindows[id].width;
  h = m_OutputWindows[id].height;
}

void D3D12DebugManager::ClearOutputWindowColour(uint64_t id, float col[4])
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  m_WrappedDevice->GetList()->Reset(m_WrappedDevice->GetAlloc(), NULL);

  m_WrappedDevice->GetList()->ClearRenderTargetView(Unwrap(m_OutputWindows[id].rtv), col, 0, NULL);

  m_WrappedDevice->GetList()->Close();

  ID3D12CommandList *list = (ID3D12CommandList *)m_WrappedDevice->GetList();
  m_WrappedDevice->GetQueue()->GetReal()->ExecuteCommandLists(1, &list);
}

void D3D12DebugManager::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  m_WrappedDevice->GetList()->Reset(m_WrappedDevice->GetAlloc(), NULL);

  m_WrappedDevice->GetList()->ClearDepthStencilView(
      Unwrap(m_OutputWindows[id].dsv), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth,
      stencil, 0, NULL);

  m_WrappedDevice->GetList()->Close();

  ID3D12CommandList *list = (ID3D12CommandList *)m_WrappedDevice->GetList();
  m_WrappedDevice->GetQueue()->GetReal()->ExecuteCommandLists(1, &list);
}

void D3D12DebugManager::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.bb == NULL)
    return;

  m_width = (int32_t)outw.width;
  m_height = (int32_t)outw.height;

  D3D12_RESOURCE_BARRIER barrier;
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = Unwrap(m_OutputWindows[id].bb);
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

  m_WrappedDevice->GetList()->Reset(m_WrappedDevice->GetAlloc(), NULL);

  m_WrappedDevice->GetList()->ResourceBarrier(1, &barrier);

  m_WrappedDevice->GetList()->Close();

  ID3D12CommandList *list = (ID3D12CommandList *)m_WrappedDevice->GetList();
  m_WrappedDevice->GetQueue()->GetReal()->ExecuteCommandLists(1, &list);
}

bool D3D12DebugManager::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

void D3D12DebugManager::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_OutputWindows[id].swap)
  {
    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = Unwrap(m_OutputWindows[id].bb);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    m_WrappedDevice->GetList()->Reset(m_WrappedDevice->GetAlloc(), NULL);

    m_WrappedDevice->GetList()->ResourceBarrier(1, &barrier);

    m_WrappedDevice->GetList()->Close();

    ID3D12CommandList *list = (ID3D12CommandList *)m_WrappedDevice->GetList();
    m_WrappedDevice->GetQueue()->GetReal()->ExecuteCommandLists(1, &list);

    m_OutputWindows[id].swap->Present(0, 0);
  }
}
