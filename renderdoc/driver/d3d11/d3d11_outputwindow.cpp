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

#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"

void D3D11Replay::OutputWindow::MakeRTV()
{
  ID3D11Texture2D *texture = NULL;
  HRESULT hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to get swap chain buffer, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(texture);
    return;
  }

  hr = dev->CreateRenderTargetView(texture, NULL, &rtv);

  SAFE_RELEASE(texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RTV for swap chain buffer, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(swap);
    return;
  }
}

void D3D11Replay::OutputWindow::MakeDSV()
{
  ID3D11Texture2D *texture = NULL;
  HRESULT hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to get swap chain buffer, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(texture);
    return;
  }

  D3D11_TEXTURE2D_DESC texDesc;
  texture->GetDesc(&texDesc);

  SAFE_RELEASE(texture);

  texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

  hr = dev->CreateTexture2D(&texDesc, NULL, &texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV texture for main output, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(swap);
    SAFE_RELEASE(rtv);
    return;
  }

  hr = dev->CreateDepthStencilView(texture, NULL, &dsv);

  SAFE_RELEASE(texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV for main output, HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(swap);
    SAFE_RELEASE(rtv);
    return;
  }
}

uint64_t D3D11Replay::MakeOutputWindow(WindowingData window, bool depth)
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

  hr = m_pFactory->CreateSwapChain(m_pDevice, &swapDesc, &outw.swap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create swap chain for HWND, HRESULT: %s", ToStr(hr).c_str());
    return 0;
  }

  outw.MakeRTV();

  outw.dsv = NULL;
  if(depth)
    outw.MakeDSV();

  uint64_t id = m_OutputWindowID++;
  m_OutputWindows[id] = outw;
  return id;
}

void D3D11Replay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  SAFE_RELEASE(outw.swap);
  SAFE_RELEASE(outw.rtv);
  SAFE_RELEASE(outw.dsv);

  m_OutputWindows.erase(it);
}

bool D3D11Replay::CheckResizeOutputWindow(uint64_t id)
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

    D3D11RenderStateTracker tracker(m_pImmediateContext);

    m_pImmediateContext->OMSetRenderTargets(0, 0, 0);

    if(outw.width > 0 && outw.height > 0)
    {
      SAFE_RELEASE(outw.rtv);
      SAFE_RELEASE(outw.dsv);

      DXGI_SWAP_CHAIN_DESC desc;
      outw.swap->GetDesc(&desc);

      HRESULT hr = outw.swap->ResizeBuffers(desc.BufferCount, outw.width, outw.height,
                                            desc.BufferDesc.Format, desc.Flags);

      if(FAILED(hr))
      {
        RDCERR("Failed to resize swap chain, HRESULT: %s", ToStr(hr).c_str());
        return true;
      }

      outw.MakeRTV();
      outw.MakeDSV();
    }

    return true;
  }

  return false;
}

void D3D11Replay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  w = m_OutputWindows[id].width;
  h = m_OutputWindows[id].height;
}

void D3D11Replay::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  m_pImmediateContext->ClearRenderTargetView(m_OutputWindows[id].rtv, &col.x);
}

void D3D11Replay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_OutputWindows[id].dsv)
    m_pImmediateContext->ClearDepthStencilView(
        m_OutputWindows[id].dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void D3D11Replay::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_RealState.active)
    RDCERR("Trashing RealState! Mismatched use of BindOutputWindow / FlipOutputWindow");

  m_RealState.active = true;
  m_RealState.state.CopyState(*m_pImmediateContext->GetCurrentPipelineState());

  m_pImmediateContext->OMSetRenderTargets(
      1, &m_OutputWindows[id].rtv, depth && m_OutputWindows[id].dsv ? m_OutputWindows[id].dsv : NULL);

  D3D11_VIEWPORT viewport = {
      0, 0, (float)m_OutputWindows[id].width, (float)m_OutputWindows[id].height, 0.0f, 1.0f};
  m_pImmediateContext->RSSetViewports(1, &viewport);

  SetOutputDimensions(m_OutputWindows[id].width, m_OutputWindows[id].height);
}

bool D3D11Replay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

void D3D11Replay::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_OutputWindows[id].swap)
    m_OutputWindows[id].swap->Present(0, 0);

  if(m_RealState.active)
  {
    m_RealState.active = false;
    m_RealState.state.ApplyState(m_pImmediateContext);
    m_RealState.state.Clear();
  }
  else
  {
    RDCERR("RealState wasn't active! Mismatched use of BindOutputWindow / FlipOutputWindow");
  }
}
