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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"

class WrappedID3D12Device;
class D3D12ResourceManager;

class D3D12DebugManager
{
public:
  D3D12DebugManager(WrappedID3D12Device *wrapper);
  ~D3D12DebugManager();

  uint64_t MakeOutputWindow(void *w, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColour(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void SetOutputDimensions(int w, int h)
  {
    m_width = w;
    m_height = h;
  }
  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }
private:
  struct OutputWindow
  {
    HWND wnd;
    IDXGISwapChain *swap;
    ID3D12Resource *bb[2];
    uint32_t bbIdx;
    ID3D12Resource *col;
    ID3D12Resource *depth;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;

    WrappedID3D12Device *dev;

    void MakeRTV(bool multisampled);
    void MakeDSV();

    int width, height;
  };

  ID3D12DescriptorHeap *rtvHeap;
  ID3D12DescriptorHeap *dsvHeap;

  int m_width, m_height;

  uint64_t m_OutputWindowID;
  map<uint64_t, OutputWindow> m_OutputWindows;

  WrappedID3D12Device *m_WrappedDevice;
  ID3D12Device *m_Device;

  IDXGIFactory4 *m_pFactory;

  D3D12ResourceManager *m_ResourceManager;
};
