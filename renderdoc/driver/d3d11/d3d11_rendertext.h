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

#include "driver/dx/official/d3d11_4.h"

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;

class D3D11TextRenderer
{
public:
  D3D11TextRenderer(WrappedID3D11Device *wrapper);
  ~D3D11TextRenderer();

  void SetOutputDimensions(int w, int h)
  {
    m_width = w;
    m_height = h;
  }
  int GetWidth() { return RDCMAX(1, m_width); }
  int GetHeight() { return RDCMAX(1, m_height); }
  void SetOutputWindow(HWND w);

  void RenderText(float x, float y, const char *textfmt, ...);

private:
  int m_width = 1, m_height = 1;
  float m_supersamplingX = 1.0f, m_supersamplingY = 1.0f;

  WrappedID3D11Device *m_pDevice = NULL;
  WrappedID3D11DeviceContext *m_pImmediateContext = NULL;

  void RenderTextInternal(float x, float y, const char *text);

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  ID3D11BlendState *BlendState = NULL;
  ID3D11SamplerState *LinearSampler = NULL;
  ID3D11ShaderResourceView *Tex = NULL;
  ID3D11Buffer *CBuffer = NULL;
  ID3D11Buffer *GlyphData = NULL;
  ID3D11Buffer *CharBuffer = NULL;
  ID3D11VertexShader *VS = NULL;
  ID3D11PixelShader *PS = NULL;

  // only used on FEATURE_LEVEL_9_x rendering
  ID3D11InputLayout *Layout = NULL;
  ID3D11Buffer *FL9Buffer = NULL;

  float CharAspect = 1.0f;
  float CharSize = 1.0f;
};