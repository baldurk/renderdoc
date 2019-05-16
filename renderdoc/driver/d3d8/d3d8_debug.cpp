/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d8_debug.h"
#include "os/os_specific.h"
#include "stb/stb_truetype.h"

D3D8DebugManager::D3D8DebugManager(WrappedD3DDevice8 *wrapper)
    : m_WrappedDevice(wrapper), m_fvf(D3DFVF_XYZ | D3DFVF_TEX1)
{
  InitFontRendering();
}

D3D8DebugManager::~D3D8DebugManager()
{
  ShutdownFontRendering();
}

bool D3D8DebugManager::InitFontRendering()
{
  HRESULT hr = S_OK;

  int width = FONT_TEX_WIDTH;
  int height = FONT_TEX_HEIGHT;

  std::string font = GetEmbeddedResource(sourcecodepro_ttf);
  byte *ttfdata = (byte *)font.c_str();

  const int firstChar = int(' ') + 1;
  const int lastChar = 127;
  const int numChars = lastChar - firstChar;

  byte *buf = new byte[width * height];

  const float pixelHeight = 20.0f;

  stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars,
                       m_Font.charData);

  stbtt_fontinfo f = {0};
  stbtt_InitFont(&f, ttfdata, 0);

  int ascent = 0;
  stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

  m_Font.maxHeight = float(ascent) * stbtt_ScaleForPixelHeight(&f, pixelHeight);

  IDirect3DTexture8 *fontTex = NULL;

  hr = m_WrappedDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &fontTex);

  if(FAILED(hr))
  {
    RDCERR("Failed to create font texture HRESULT: %s", ToStr(hr).c_str());
  }

  D3DLOCKED_RECT lockedRegion;
  hr = fontTex->LockRect(0, &lockedRegion, NULL, D3DLOCK_DISCARD);

  if(FAILED(hr))
  {
    RDCERR("Failed to lock font texture HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    BYTE *texBase = (BYTE *)lockedRegion.pBits;

    for(int y = 0; y < height; y++)
    {
      byte *curRow = (texBase + (y * lockedRegion.Pitch));

      for(int x = 0; x < width; x++)
      {
        curRow[x * 4 + 0] = buf[(y * width) + x];
        curRow[x * 4 + 1] = buf[(y * width) + x];
        curRow[x * 4 + 2] = buf[(y * width) + x];
        curRow[x * 4 + 3] = buf[(y * width) + x];
      }
    }

    hr = fontTex->UnlockRect(0);
    if(hr != S_OK)
    {
      RDCERR("Failed to unlock font texture HRESULT: %s", ToStr(hr).c_str());
    }
  }

  m_Font.Tex = fontTex;

  delete[] buf;

  return true;
}

void D3D8DebugManager::ShutdownFontRendering()
{
  SAFE_RELEASE(m_Font.Tex);
}

void D3D8DebugManager::SetOutputWindow(HWND w)
{
  // RECT rect;
  // GetClientRect(w, &rect);
  // m_supersamplingX = float(m_width) / float(rect.right - rect.left);
  // m_supersamplingY = float(m_height) / float(rect.bottom - rect.top);
}

void D3D8DebugManager::RenderText(float x, float y, const char *textfmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, textfmt);
  StringFormat::vsnprintf(tmpBuf, 4095, textfmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderTextInternal(x, y, tmpBuf);
}

void D3D8DebugManager::RenderTextInternal(float x, float y, const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderTextInternal(x, y, text);
    RenderTextInternal(x, y + 1.0f, t + 1);
    *t = '\n';
    return;
  }

  if(strlen(text) == 0)
    return;

  RDCASSERT(strlen(text) < FONT_MAX_CHARS);

  // transforms
  float width = (float)m_width;
  float height = (float)m_height;
  float nearPlane = 0.001f;
  float farPlane = 1.f;
  D3DMATRIX identity = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                        0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
  D3DMATRIX ortho = {2.f / width,
                     0.f,
                     0.f,
                     0.f,
                     0.f,
                     -(2.f / height),
                     0.f,
                     0.f,
                     0.f,
                     0.f,
                     1.f / (farPlane - nearPlane),
                     0.f,
                     0.f,
                     0.f,
                     nearPlane / (nearPlane - farPlane),
                     1.f};

  HRESULT res = S_OK;
  res |= m_WrappedDevice->SetTransform(D3DTS_PROJECTION, &ortho);
  res |= m_WrappedDevice->SetTransform(D3DTS_WORLD, &identity);
  res |= m_WrappedDevice->SetTransform(D3DTS_VIEW, &identity);

  // enable fixed function pipeline
  res |= m_WrappedDevice->SetVertexShader(NULL);
  res |= m_WrappedDevice->SetPixelShader(NULL);

  // default render states
  res |= m_WrappedDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_CLIPPING, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
  res |= m_WrappedDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  res |= m_WrappedDevice->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
  res |= m_WrappedDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);

  // texture stage states
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR /*D3DTEXF_POINT*/);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR /*D3DTEXF_POINT*/);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR /*D3DTEXF_POINT*/);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_COLORARG0, D3DTA_CURRENT);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_ALPHAARG0, D3DTA_CURRENT);
  res |= m_WrappedDevice->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_CURRENT);

  res |= m_WrappedDevice->SetVertexShader(m_fvf);
  res |= m_WrappedDevice->SetTexture(0, m_Font.Tex);
  for(uint32_t stage = 1; stage < 8; stage++)
  {
    res |= m_WrappedDevice->SetTexture(stage, NULL);
  }

  struct Vertex
  {
    float pos[3];
    float uv[2];

    Vertex() {}
    Vertex(float posx, float posy, float posz, float texu, float texv)
    {
      pos[0] = posx;
      pos[1] = posy;
      pos[2] = posz;
      uv[0] = texu;
      uv[1] = texv;
    }
  };

  struct Quad
  {
    Vertex vertices[6];

    Quad() {}
    Quad(float x0, float y0, float z, float s0, float t0, float x1, float y1, float s1, float t1)
    {
      vertices[0] = Vertex(x0, y0, z, s0, t0);
      vertices[1] = Vertex(x1, y0, z, s1, t0);
      vertices[2] = Vertex(x0, y1, z, s0, t1);
      vertices[3] = Vertex(x1, y0, z, s1, t0);
      vertices[4] = Vertex(x1, y1, z, s1, t1);
      vertices[5] = Vertex(x0, y1, z, s0, t1);
    }
  };

  Quad *quads = NULL;
  Quad background;
  UINT triangleCount = 0;
  {
    UINT quadCount = (UINT)strlen(text);    // calculate string length

    triangleCount = quadCount * 2;
    // create text VB
    quads = new Quad[quadCount];

    float textStartingPositionX = (-width / 2.f) + (x * m_Font.charData->xadvance);
    float textStartingPositionY = (-height / 2.f) + ((y + 1.f) * m_Font.maxHeight);

    float textPositionX = textStartingPositionX;
    float textPositionY = textStartingPositionY;

    for(UINT i = 0; i < quadCount; ++i)
    {
      char glyphIndex = text[i] - (' ' + 1);
      if(glyphIndex < 0)
      {
        float currentX = textPositionX;
        textPositionX += m_Font.charData->xadvance;
        quads[i] = Quad(currentX, textPositionY - m_Font.maxHeight, 0.5f, 0.f, 0.f, textPositionX,
                        textPositionY, 0.f, 0.f);
      }
      else
      {
        stbtt_aligned_quad quad;
        stbtt_GetBakedQuad(m_Font.charData, 256, 128, glyphIndex, &textPositionX, &textPositionY,
                           &quad, 0);
        quads[i] = Quad(quad.x0, quad.y0, 0.5f, quad.s0, quad.t0, quad.x1, quad.y1, quad.s1, quad.t1);
      }
    }

    background = Quad(textStartingPositionX, textStartingPositionY - m_Font.maxHeight, 0.6f, 0.f,
                      0.f, textPositionX, textPositionY + 3.f, 0.f, 0.f);
  }

  if(quads != NULL)
  {
    // overlay render states
    res |= m_WrappedDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    res |= m_WrappedDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, &background, sizeof(Vertex));

    //// overlay render states
    res |= m_WrappedDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    res |= m_WrappedDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    res |= m_WrappedDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    res |= m_WrappedDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, triangleCount, &quads[0],
                                            sizeof(Vertex));
    delete[] quads;
  }
}