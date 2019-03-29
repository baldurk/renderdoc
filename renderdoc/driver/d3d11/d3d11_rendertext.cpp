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

#include "d3d11_rendertext.h"
#include "maths/matrix.h"
#include "stb/stb_truetype.h"
#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"
#include "d3d11_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

static Vec2f quadPos[] = {
    Vec2f(0.0f, 0.0f), Vec2f(1.0f, 0.0f), Vec2f(0.0f, 1.0f), Vec2f(1.0f, 1.0f),
};

D3D11TextRenderer::D3D11TextRenderer(WrappedID3D11Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D11TextRenderer));

  m_pDevice = wrapper;
  m_pImmediateContext = m_pDevice->GetImmediateContext();

  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  HRESULT hr = S_OK;

  D3D11_BLEND_DESC blendDesc;
  RDCEraseEl(blendDesc);

  blendDesc.AlphaToCoverageEnable = FALSE;
  blendDesc.IndependentBlendEnable = FALSE;
  blendDesc.RenderTarget[0].BlendEnable = TRUE;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  hr = m_pDevice->CreateBlendState(&blendDesc, &BlendState);

  if(FAILED(hr))
    RDCERR("Failed to create font blendstate HRESULT: %s", ToStr(hr).c_str());

  if(BlendState)
    m_pDevice->InternalRef();
  rm->SetInternalResource(BlendState);

  D3D11_SAMPLER_DESC sampDesc;
  RDCEraseEl(sampDesc);

  sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  sampDesc.MaxAnisotropy = 1;
  sampDesc.MinLOD = 0;
  sampDesc.MaxLOD = FLT_MAX;
  sampDesc.MipLODBias = 0.0f;

  hr = m_pDevice->CreateSamplerState(&sampDesc, &LinearSampler);

  if(FAILED(hr))
    RDCERR("Failed to create linear sampler state HRESULT: %s", ToStr(hr).c_str());

  if(LinearSampler)
    m_pDevice->InternalRef();
  rm->SetInternalResource(LinearSampler);

  D3D11_TEXTURE2D_DESC desc;
  RDCEraseEl(desc);

  int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

  desc.ArraySize = 1;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = 0;
  desc.Format = DXGI_FORMAT_R8_UNORM;
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.MiscFlags = 0;
  desc.SampleDesc.Quality = 0;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;

  std::string font = GetEmbeddedResource(sourcecodepro_ttf);
  byte *ttfdata = (byte *)font.c_str();

  const int firstChar = int(' ') + 1;
  const int lastChar = 127;
  const int numChars = lastChar - firstChar;

  byte *buf = new byte[width * height];

  const float pixelHeight = 20.0f;

  stbtt_bakedchar chardata[numChars];
  stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars, chardata);

  CharSize = pixelHeight;
  CharAspect = chardata->xadvance / pixelHeight;

  stbtt_fontinfo f = {0};
  stbtt_InitFont(&f, ttfdata, 0);

  int ascent = 0;
  stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

  float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, pixelHeight);

  D3D11_SUBRESOURCE_DATA initialData;
  initialData.pSysMem = buf;
  initialData.SysMemPitch = width;
  initialData.SysMemSlicePitch = width * height;

  ID3D11Texture2D *debugTex = NULL;

  hr = m_pDevice->CreateTexture2D(&desc, &initialData, &debugTex);

  if(FAILED(hr))
    RDCERR("Failed to create debugTex HRESULT: %s", ToStr(hr).c_str());

  if(debugTex)
    m_pDevice->InternalRef();
  rm->SetInternalResource(debugTex);

  delete[] buf;

  hr = m_pDevice->CreateShaderResourceView(debugTex, NULL, &Tex);

  if(FAILED(hr))
    RDCERR("Failed to create Tex HRESULT: %s", ToStr(hr).c_str());

  if(Tex)
    m_pDevice->InternalRef();
  rm->SetInternalResource(Tex);

  SAFE_RELEASE(debugTex);

  Vec4f glyphData[2 * (numChars + 1)];

  D3D11_BUFFER_DESC cbufDesc;

  cbufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbufDesc.Usage = D3D11_USAGE_DYNAMIC;
  cbufDesc.StructureByteStride = 0;
  cbufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  cbufDesc.MiscFlags = 0;

  cbufDesc.ByteWidth = sizeof(glyphData);
  hr = m_pDevice->CreateBuffer(&cbufDesc, NULL, &GlyphData);

  if(FAILED(hr))
    RDCERR("Failed to create font GlyphData HRESULT: %s", ToStr(hr).c_str());

  if(GlyphData)
    m_pDevice->InternalRef();
  rm->SetInternalResource(GlyphData);

  for(int i = 0; i < numChars; i++)
  {
    stbtt_bakedchar *b = chardata + i;

    float x = b->xoff;
    float y = b->yoff + maxheight;

    glyphData[(i + 1) * 2 + 0] =
        Vec4f(x / b->xadvance, y / pixelHeight, b->xadvance / float(b->x1 - b->x0),
              pixelHeight / float(b->y1 - b->y0));
    glyphData[(i + 1) * 2 + 1] = Vec4f(b->x0, b->y0, b->x1, b->y1);
  }

  D3D11_MAPPED_SUBRESOURCE mapped;

  hr = m_pImmediateContext->Map(GlyphData, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    memcpy(mapped.pData, glyphData, sizeof(glyphData));
    m_pImmediateContext->Unmap(GlyphData, 0);
  }

  cbufDesc.ByteWidth = sizeof(FontCBuffer);
  hr = m_pDevice->CreateBuffer(&cbufDesc, NULL, &CBuffer);

  if(FAILED(hr))
    RDCERR("Failed to create font CBuffer HRESULT: %s", ToStr(hr).c_str());

  if(CBuffer)
    m_pDevice->InternalRef();
  rm->SetInternalResource(CBuffer);

  cbufDesc.ByteWidth = (2 + FONT_MAX_CHARS) * sizeof(uint32_t) * 4;
  hr = m_pDevice->CreateBuffer(&cbufDesc, NULL, &CharBuffer);

  if(FAILED(hr))
    RDCERR("Failed to create font CharBuffer HRESULT: %s", ToStr(hr).c_str());

  if(CharBuffer)
    m_pDevice->InternalRef();
  rm->SetInternalResource(CharBuffer);

  std::string hlsl = GetEmbeddedResource(text_hlsl);

  D3D11ShaderCache *shaderCache = wrapper->GetShaderCache();

  shaderCache->SetCaching(true);

  if(m_pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0)
  {
    VS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_TextVS", "vs_4_0");

    if(VS)
      m_pDevice->InternalRef();
    rm->SetInternalResource(VS);

    PS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TextPS", "ps_4_0");

    if(PS)
      m_pDevice->InternalRef();
    rm->SetInternalResource(PS);
  }
  else
  {
    D3D11_INPUT_ELEMENT_DESC inputs[] = {
        // quad position xy, instance ID, and character, packed into Vec4f
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    // if we're on 9_x feature levels, compile the shaders in 9_0 mode and switch to the downlevel
    // vertex shader that expects vertex inputs directly rather than using SV_VertexID/SV_InstanceID
    VS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_Text9VS", "vs_4_0_level_9_0", 1, inputs,
                                  &Layout);

    if(VS)
      m_pDevice->InternalRef();
    rm->SetInternalResource(VS);

    if(Layout)
      m_pDevice->InternalRef();
    rm->SetInternalResource(Layout);

    PS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TextPS", "ps_4_0_level_9_0");

    if(PS)
      m_pDevice->InternalRef();
    rm->SetInternalResource(PS);

    // these buffers are immutable because they're just replacing the fixed shader generation on
    // FL10+
    D3D11_BUFFER_DESC vbufDesc;
    vbufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbufDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbufDesc.StructureByteStride = 0;
    vbufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbufDesc.MiscFlags = 0;
    vbufDesc.ByteWidth = sizeof(Vec4f) * 6 * FONT_MAX_CHARS;

    hr = m_pDevice->CreateBuffer(&vbufDesc, NULL, &FL9Buffer);

    if(FAILED(hr))
      RDCERR("Failed to create FL9 text PosBuffer HRESULT: %s", ToStr(hr).c_str());

    if(FL9Buffer)
      m_pDevice->InternalRef();
    rm->SetInternalResource(FL9Buffer);
  }

  shaderCache->SetCaching(false);
}

D3D11TextRenderer::~D3D11TextRenderer()
{
  SAFE_RELEASE(Tex);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(LinearSampler);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(BlendState);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(CBuffer);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(GlyphData);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(CharBuffer);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(VS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(PS);
  m_pDevice->InternalRelease();

  SAFE_RELEASE(Layout);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(FL9Buffer);
  m_pDevice->InternalRelease();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void D3D11TextRenderer::SetOutputWindow(HWND w)
{
  RECT rect = {0, 0, 0, 0};
  GetClientRect(w, &rect);
  if(rect.right == rect.left || rect.bottom == rect.top)
  {
    m_supersamplingX = 1.0f;
    m_supersamplingY = 1.0f;
  }
  else
  {
    m_supersamplingX = float(m_width) / float(rect.right - rect.left);
    m_supersamplingY = float(m_height) / float(rect.bottom - rect.top);
  }
}

void D3D11TextRenderer::RenderText(float x, float y, const char *textfmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, textfmt);
  StringFormat::vsnprintf(tmpBuf, 4095, textfmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderTextInternal(x, y, tmpBuf);
}

void D3D11TextRenderer::RenderTextInternal(float x, float y, const char *text)
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

  if(!VS || !PS)
    return;

  RDCASSERT(strlen(text) < FONT_MAX_CHARS);

  FontCBuffer data = {};

  data.TextPosition.x = x;
  data.TextPosition.y = y;

  data.FontScreenAspect.x = 1.0f / float(GetWidth());
  data.FontScreenAspect.y = 1.0f / float(GetHeight());

  data.TextSize = CharSize;
  data.FontScreenAspect.x *= CharAspect;

  data.FontScreenAspect.x *= m_supersamplingX;
  data.FontScreenAspect.y *= m_supersamplingY;

  data.CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
  data.CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->Map(CBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  memcpy(mapped.pData, &data, sizeof(data));
  m_pImmediateContext->Unmap(CBuffer, 0);

  // are we in fl9? need to upload the characters as floats into a VB instead of uints into a CB
  bool modern = (FL9Buffer == NULL);

  if(modern)
    hr = m_pImmediateContext->Map(CharBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  else
    hr = m_pImmediateContext->Map(FL9Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map charbuffer HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  if(modern)
  {
    unsigned long *texs = (unsigned long *)mapped.pData;

    for(size_t i = 0; i < strlen(text); i++)
      texs[i * 4] = (text[i] - ' ');
  }
  else
  {
    Vec4f *texs = (Vec4f *)mapped.pData;

    for(size_t i = 0; i < strlen(text); i++)
    {
      texs[i * 6 + 0] = Vec4f(quadPos[0].x, quadPos[0].y, float(i), float(text[i] - ' '));
      texs[i * 6 + 1] = Vec4f(quadPos[1].x, quadPos[1].y, float(i), float(text[i] - ' '));
      texs[i * 6 + 2] = Vec4f(quadPos[2].x, quadPos[2].y, float(i), float(text[i] - ' '));

      texs[i * 6 + 3] = Vec4f(quadPos[1].x, quadPos[1].y, float(i), float(text[i] - ' '));
      texs[i * 6 + 4] = Vec4f(quadPos[3].x, quadPos[3].y, float(i), float(text[i] - ' '));
      texs[i * 6 + 5] = Vec4f(quadPos[2].x, quadPos[2].y, float(i), float(text[i] - ' '));
    }
  }

  if(modern)
    m_pImmediateContext->Unmap(CharBuffer, 0);
  else
    m_pImmediateContext->Unmap(FL9Buffer, 0);

  // can't just clear state because we need to keep things like render targets.
  {
    m_pImmediateContext->IASetPrimitiveTopology(modern ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                       : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // we can set Layout unconditionally - it is NULL for FL10+
    m_pImmediateContext->IASetInputLayout(Layout);

    // we can also set the vertex buffers unconditionally
    UINT strides[1] = {sizeof(Vec4f)};
    UINT offsets[1] = {};
    m_pImmediateContext->IASetVertexBuffers(0, 1, &FL9Buffer, strides, offsets);

    m_pImmediateContext->VSSetShader(VS, NULL, 0);
    m_pImmediateContext->VSSetConstantBuffers(0, 1, &CBuffer);
    m_pImmediateContext->VSSetConstantBuffers(1, 1, &GlyphData);
    m_pImmediateContext->VSSetConstantBuffers(2, 1, &CharBuffer);

    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);

    m_pImmediateContext->RSSetState(NULL);

    D3D11_VIEWPORT view;
    view.TopLeftX = 0;
    view.TopLeftY = 0;
    view.Width = (float)GetWidth();
    view.Height = (float)GetHeight();
    view.MinDepth = 0.0f;
    view.MaxDepth = 1.0f;
    m_pImmediateContext->RSSetViewports(1, &view);

    m_pImmediateContext->PSSetShader(PS, NULL, 0);
    m_pImmediateContext->PSSetShaderResources(0, 1, &Tex);

    m_pImmediateContext->PSSetSamplers(1, 1, &LinearSampler);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_pImmediateContext->OMSetBlendState(BlendState, factor, 0xffffffff);

    if(modern)
      m_pImmediateContext->DrawInstanced(4, (uint32_t)strlen(text), 0, 0);
    else
      m_pImmediateContext->Draw(6 * (uint32_t)strlen(text), 0);
  }
}