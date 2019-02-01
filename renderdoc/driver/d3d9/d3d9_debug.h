/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2016-2019 Baldur Karlsson
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

#include <list>
#include <map>
#include <utility>
#include "driver/dx/official/d3d9.h"
#include "stb/stb_truetype.h"
#include "d3d9_common.h"
#include "d3d9_device.h"

class D3D9DebugManager
{
public:
  D3D9DebugManager(WrappedD3DDevice9 *wrapper);
  ~D3D9DebugManager();

  void RenderText(float x, float y, const char *textfmt, ...);

  void SetOutputDimensions(int w, int h)
  {
    m_width = w;
    m_height = h;
  }
  void SetOutputWindow(HWND w);

  // font/text rendering
  bool InitFontRendering();
  void ShutdownFontRendering();

  void RenderTextInternal(float x, float y, const char *text);

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  static const uint32_t STAGE_BUFFER_BYTE_SIZE = 4 * 1024 * 1024;

  struct FontData
  {
    FontData() { RDCEraseMem(this, sizeof(FontData)); }
    ~FontData() { SAFE_RELEASE(Tex); }
    IDirect3DTexture9 *Tex;
    stbtt_bakedchar charData[FONT_MAX_CHARS];
    float maxHeight;
  } m_Font;

  DWORD m_fvf;

  int m_width = 0;
  int m_height = 0;
  WrappedD3DDevice9 *m_WrappedDevice;
};
