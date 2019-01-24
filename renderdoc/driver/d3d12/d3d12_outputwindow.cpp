/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"

void D3D12Replay::OutputWindow::MakeRTV(bool multisampled)
{
  SAFE_RELEASE(col);
  SAFE_RELEASE(colResolve);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.Alignment = 0;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  texDesc.SampleDesc.Count = multisampled ? D3D12_MSAA_SAMPLECOUNT : 1;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = S_OK;

  hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                    __uuidof(ID3D12Resource), (void **)&col);

  col->SetName(L"Output Window RTV");

  if(FAILED(hr))
  {
    RDCERR("Failed to create colour texture for window, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  colResolve = NULL;

  if(multisampled)
  {
    texDesc.SampleDesc.Count = 1;

    hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                      __uuidof(ID3D12Resource), (void **)&colResolve);

    col->SetName(L"Output Window Resolve");

    if(FAILED(hr))
    {
      RDCERR("Failed to create resolve texture for window, HRESULT: %s", ToStr(hr).c_str());
      return;
    }
  }

  dev->CreateRenderTargetView(col, NULL, rtv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RTV for main window, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(colResolve);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

void D3D12Replay::OutputWindow::MakeDSV()
{
  SAFE_RELEASE(depth);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.Alignment = 0;
  texDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;
  texDesc.Format = DXGI_FORMAT_D32_FLOAT;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                            D3D12_RESOURCE_STATE_DEPTH_WRITE, NULL,
                                            __uuidof(ID3D12Resource), (void **)&depth);

  col->SetName(L"Output Window Depth");

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV texture for output window, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  dev->CreateDepthStencilView(depth, NULL, dsv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV for output window, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(colResolve);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

uint64_t D3D12Replay::MakeOutputWindow(WindowingData window, bool depth)
{
  RDCASSERT(window.system == WindowingSystem::Win32, window.system);

  OutputWindow outw;
  outw.wnd = window.win32.window;
  outw.dev = m_pDevice;

  DXGI_SWAP_CHAIN_DESC swapDesc;
  RDCEraseEl(swapDesc);

  RECT rect;
  GetClientRect(outw.wnd, &rect);

  swapDesc.BufferCount = 2;
  swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  outw.width = swapDesc.BufferDesc.Width = rect.right - rect.left;
  outw.height = swapDesc.BufferDesc.Height = rect.bottom - rect.top;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.SampleDesc.Count = 1;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.OutputWindow = outw.wnd;
  swapDesc.Windowed = TRUE;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapDesc.Flags = 0;

  HRESULT hr = S_OK;

  hr = m_pFactory->CreateSwapChain(m_pDevice->GetQueue(), &swapDesc, &outw.swap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create swap chain for HWND, HRESULT: %s", ToStr(hr).c_str());
    return 0;
  }

  outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb[0]);
  outw.swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&outw.bb[1]);

  outw.bbIdx = 0;

  outw.rtv = GetDebugManager()->GetCPUHandle(FIRST_WIN_RTV);
  outw.rtv.ptr += SIZE_T(m_OutputWindowID) * sizeof(D3D12Descriptor);

  outw.dsv = GetDebugManager()->GetCPUHandle(FIRST_WIN_DSV);
  outw.dsv.ptr += SIZE_T(m_DSVID) * sizeof(D3D12Descriptor);

  outw.col = NULL;
  outw.colResolve = NULL;
  outw.MakeRTV(depth);
  m_pDevice->CreateRenderTargetView(outw.col, NULL, outw.rtv);

  outw.depth = NULL;
  if(depth)
  {
    outw.MakeDSV();
    m_DSVID++;
  }

  uint64_t id = m_OutputWindowID++;
  m_OutputWindows[id] = outw;
  return id;
}

void D3D12Replay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  m_pDevice->FlushLists(true);

  SAFE_RELEASE(outw.swap);
  SAFE_RELEASE(outw.bb[0]);
  SAFE_RELEASE(outw.bb[1]);
  SAFE_RELEASE(outw.col);
  SAFE_RELEASE(outw.colResolve);
  SAFE_RELEASE(outw.depth);

  m_OutputWindows.erase(it);
}

bool D3D12Replay::CheckResizeOutputWindow(uint64_t id)
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

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists(true);

    if(outw.width > 0 && outw.height > 0)
    {
      SAFE_RELEASE(outw.bb[0]);
      SAFE_RELEASE(outw.bb[1]);

      DXGI_SWAP_CHAIN_DESC desc;
      outw.swap->GetDesc(&desc);

      HRESULT hr = outw.swap->ResizeBuffers(desc.BufferCount, outw.width, outw.height,
                                            desc.BufferDesc.Format, desc.Flags);

      if(FAILED(hr))
      {
        RDCERR("Failed to resize swap chain, HRESULT: %s", ToStr(hr).c_str());
        return true;
      }

      outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb[0]);
      outw.swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&outw.bb[1]);

      outw.bbIdx = 0;

      if(outw.depth)
      {
        outw.MakeRTV(true);
        outw.MakeDSV();
      }
      else
      {
        outw.MakeRTV(false);
      }
    }

    return true;
  }

  return false;
}

void D3D12Replay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  w = m_OutputWindows[id].width;
  h = m_OutputWindows[id].height;
}

void D3D12Replay::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  list->ClearRenderTargetView(m_OutputWindows[id].rtv, &col.x, 0, NULL);

  list->Close();
}

void D3D12Replay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  list->ClearDepthStencilView(m_OutputWindows[id].dsv,
                              D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0,
                              NULL);

  list->Close();
}

void D3D12Replay::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  m_CurrentOutputWindow = id;

  if(outw.bb[0] == NULL)
    return;

  SetOutputDimensions(outw.width, outw.height);
}

bool D3D12Replay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

void D3D12Replay::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  if(m_OutputWindows[id].bb[0] == NULL)
    return;

  D3D12_RESOURCE_BARRIER barriers[3];
  RDCEraseEl(barriers);

  barriers[0].Transition.pResource = outw.col;
  barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barriers[0].Transition.StateAfter =
      outw.depth ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;

  barriers[1].Transition.pResource = outw.bb[outw.bbIdx];
  barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

  barriers[2].Transition.pResource = outw.colResolve;
  barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  // resolve or copy from colour to backbuffer
  if(outw.depth)
  {
    // transition colour to resolve source, resolve target to resolve dest, backbuffer to copy dest
    list->ResourceBarrier(3, barriers);

    // resolve then copy, as the resolve can't go from SRGB to non-SRGB target
    list->ResolveSubresource(barriers[2].Transition.pResource, 0, barriers[0].Transition.pResource,
                             0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    std::swap(barriers[2].Transition.StateBefore, barriers[2].Transition.StateAfter);

    // now move the resolve target into copy source
    list->ResourceBarrier(1, &barriers[2]);

    list->CopyResource(barriers[1].Transition.pResource, barriers[2].Transition.pResource);
  }
  else
  {
    // transition colour to copy source, backbuffer to copy dest
    list->ResourceBarrier(2, barriers);

    list->CopyResource(barriers[1].Transition.pResource, barriers[0].Transition.pResource);
  }

  std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
  std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);

  // transition colour back to render target, and backbuffer back to present
  list->ResourceBarrier(2, barriers);

  list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  outw.swap->Present(0, 0);

  outw.bbIdx++;
  outw.bbIdx %= 2;
}
