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

#include "driver/dx/official/d3d12.h"
#include "d3d12_common.h"

class D3D12TextRenderer
{
public:
  D3D12TextRenderer(WrappedID3D12Device *wrapper);
  ~D3D12TextRenderer();

  void SetOutputDimensions(int w, int h, DXGI_FORMAT fmt)
  {
    m_width = w;
    m_height = h;

    if(fmt == DXGI_FORMAT_B8G8R8A8_UNORM)
      m_BBFmtIdx = BGRA8_BACKBUFFER;
    else if(fmt == DXGI_FORMAT_R16G16B16A16_FLOAT)
      m_BBFmtIdx = RGBA16_BACKBUFFER;
    else
      m_BBFmtIdx = RGBA8_BACKBUFFER;
  }
  int GetWidth() { return RDCMAX(1, m_width); }
  int GetHeight() { return RDCMAX(1, m_height); }
  void RenderText(ID3D12GraphicsCommandList *list, float x, float y, const char *textfmt, ...);

private:
  int m_width = 1, m_height = 1;
  enum BackBufferFormat
  {
    BGRA8_BACKBUFFER = 0,
    RGBA8_BACKBUFFER,
    RGBA16_BACKBUFFER,
    FMTNUM_BACKBUFFER,
  } m_BBFmtIdx;

  void RenderTextInternal(ID3D12GraphicsCommandList *list, float x, float y, const char *text);

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  // how much character space is in the ring buffer
  static const int FONT_BUFFER_CHARS = 8192;

  ID3D12Resource *Tex = NULL;
  ID3D12PipelineState *Pipe[FMTNUM_BACKBUFFER] = {NULL};
  ID3D12RootSignature *RootSig = NULL;
  ID3D12Resource *Constants = {NULL};
  ID3D12Resource *GlyphData = NULL;
  ID3D12Resource *CharBuffer = NULL;
  ID3D12DescriptorHeap *descHeap = NULL;

  size_t CharOffset = 0;
  const UINT ConstRingSize = 32;
  UINT ConstRingIdx = 0;

  float CharAspect = 1.0f;
  float CharSize = 1.0f;
};