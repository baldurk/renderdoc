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

#include "d3d12_rendertext.h"
#include "driver/dx/official/d3dcompiler.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "stb/stb_truetype.h"
#include "d3d12_device.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

D3D12TextRenderer::D3D12TextRenderer(WrappedID3D12Device *wrapper)
{
  D3D12ResourceManager *rm = wrapper->GetResourceManager();

  HRESULT hr = S_OK;

  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  desc.NodeMask = 1;
  desc.NumDescriptors = 1;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

  hr = wrapper->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&descHeap);
  wrapper->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create font descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(descHeap);

  D3D12_HEAP_PROPERTIES uploadHeap;
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
  uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  uploadHeap.CreationNodeMask = 1;
  uploadHeap.VisibleNodeMask = 1;

  D3D12_HEAP_PROPERTIES defaultHeap = uploadHeap;
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

  const int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

  D3D12_RESOURCE_DESC bufDesc;
  bufDesc.Alignment = 0;
  bufDesc.DepthOrArraySize = 1;
  bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  bufDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufDesc.Height = 1;
  bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufDesc.MipLevels = 1;
  bufDesc.SampleDesc.Count = 1;
  bufDesc.SampleDesc.Quality = 0;
  bufDesc.Width = width * height;

  ID3D12Resource *uploadBuf = NULL;

  hr = wrapper->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                        __uuidof(ID3D12Resource), (void **)&uploadBuf);
  // don't add InternalRef because this is temporary

  if(FAILED(hr))
    RDCERR("Failed to create uploadBuf HRESULT: %s", ToStr(hr).c_str());

  rm->SetInternalResource(uploadBuf);

  D3D12_RESOURCE_DESC texDesc;
  texDesc.Alignment = 0;
  texDesc.DepthOrArraySize = 1;
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  texDesc.Format = DXGI_FORMAT_R8_UNORM;
  texDesc.Height = height;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texDesc.MipLevels = 1;
  texDesc.SampleDesc.Count = 1;
  texDesc.SampleDesc.Quality = 0;
  texDesc.Width = width;

  hr = wrapper->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                        D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                        __uuidof(ID3D12Resource), (void **)&Tex);
  wrapper->InternalRef();

  Tex->SetName(L"FontTex");

  if(FAILED(hr))
    RDCERR("Failed to create FontTex HRESULT: %s", ToStr(hr).c_str());

  rm->SetInternalResource(Tex);

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

  {
    D3D12_RANGE range = {0, width * height};
    byte *ptr = NULL;
    hr = uploadBuf->Map(0, &range, (void **)&ptr);

    if(FAILED(hr))
    {
      RDCERR("Can't fill font tex upload buffer HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      memcpy(ptr, buf, width * height);
      uploadBuf->Unmap(0, &range);
    }
  }

  delete[] buf;

  ID3D12GraphicsCommandList *list = wrapper->GetNewList();

  D3D12_TEXTURE_COPY_LOCATION dst, src;

  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.pResource = Tex;
  dst.SubresourceIndex = 0;

  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.pResource = uploadBuf;
  src.PlacedFootprint.Offset = 0;
  src.PlacedFootprint.Footprint.Width = width;
  src.PlacedFootprint.Footprint.Height = height;
  src.PlacedFootprint.Footprint.Depth = 1;
  src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
  src.PlacedFootprint.Footprint.RowPitch = width;

  RDCCOMPILE_ASSERT(
      (width / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == width,
      "Width isn't aligned!");

  list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Transition.pResource = Tex;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

  list->ResourceBarrier(1, &barrier);

  list->Close();

  wrapper->ExecuteLists();
  wrapper->FlushLists();

  SAFE_RELEASE(uploadBuf);

  D3D12_CPU_DESCRIPTOR_HANDLE srv = descHeap->GetCPUDescriptorHandleForHeapStart();

  wrapper->CreateShaderResourceView(Tex, NULL, srv);

  Vec4f glyphData[2 * (numChars + 1)];

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC cbDesc;
  cbDesc.Alignment = 0;
  cbDesc.DepthOrArraySize = 1;
  cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  cbDesc.Format = DXGI_FORMAT_UNKNOWN;
  cbDesc.Height = 1;
  cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  cbDesc.MipLevels = 1;
  cbDesc.SampleDesc.Count = 1;
  cbDesc.SampleDesc.Quality = 0;

  cbDesc.Width = sizeof(glyphData);
  hr = wrapper->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                        __uuidof(ID3D12Resource), (void **)&GlyphData);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create GlyphData cbuffer! %s", ToStr(hr).c_str());

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

  {
    D3D12_RANGE range = {0, sizeof(glyphData)};
    byte *ptr = NULL;
    hr = GlyphData->Map(0, &range, (void **)&ptr);

    if(FAILED(hr))
    {
      RDCERR("Can't fill glyph data buffer HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      memcpy(ptr, &glyphData, sizeof(glyphData));
      GlyphData->Unmap(0, &range);
    }
  }

  const UINT fontDataSize = 256;
  RDCCOMPILE_ASSERT(sizeof(FontCBuffer) < fontDataSize, "FontCBuffer is larger - need to align up");

  cbDesc.Width = fontDataSize * ConstRingSize;
  hr = wrapper->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                        __uuidof(ID3D12Resource), (void **)&Constants);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create Constants cbuffer! %s", ToStr(hr).c_str());

  rm->SetInternalResource(Constants);

  cbDesc.Width = FONT_BUFFER_CHARS * sizeof(uint32_t) * 4;
  hr = wrapper->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                        __uuidof(ID3D12Resource), (void **)&CharBuffer);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create CharBuffer cbuffer! %s", ToStr(hr).c_str());

  rm->SetInternalResource(CharBuffer);

  ConstRingIdx = 0;

  std::vector<D3D12_ROOT_PARAMETER1> rootSig;
  D3D12_ROOT_PARAMETER1 param = {};

  // Constants
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.ShaderRegister = 0;
  param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  rootSig.push_back(param);

  // GlyphData
  param.Descriptor.ShaderRegister = 1;
  rootSig.push_back(param);

  // CharBuffer
  param.Descriptor.ShaderRegister = 2;
  rootSig.push_back(param);

  D3D12_DESCRIPTOR_RANGE1 srvrange = {};
  srvrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvrange.BaseShaderRegister = 0;
  srvrange.NumDescriptors = 1;
  srvrange.OffsetInDescriptorsFromTableStart = 0;

  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &srvrange;

  // font SRV
  rootSig.push_back(param);

  D3D12ShaderCache *shaderCache = wrapper->GetShaderCache();

  const D3D12_STATIC_SAMPLER_DESC samplers[2] = {
      // point
      {
          D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 1,
          D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, 0.0f, FLT_MAX, 0, 0,
          D3D12_SHADER_VISIBILITY_PIXEL,
      },

      // linear
      {
          D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 1,
          D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, 0.0f, FLT_MAX, 1, 0,
          D3D12_SHADER_VISIBILITY_PIXEL,
      },
  };

  ID3DBlob *root = shaderCache->MakeRootSig(rootSig, D3D12_ROOT_SIGNATURE_FLAG_NONE, 2, samplers);

  RDCASSERT(root);

  hr = wrapper->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                    __uuidof(ID3D12RootSignature), (void **)&RootSig);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create RootSig! %s", ToStr(hr).c_str());

  rm->SetInternalResource(RootSig);

  SAFE_RELEASE(root);

  std::string hlsl = GetEmbeddedResource(text_hlsl);

  ID3DBlob *TextVS = NULL;
  ID3DBlob *TextPS = NULL;

  shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TextVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                             "vs_5_0", &TextVS);
  shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TextPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                             "ps_5_0", &TextPS);

  RDCASSERT(TextVS);
  RDCASSERT(TextPS);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  RDCEraseEl(pipeDesc);

  pipeDesc.pRootSignature = RootSig;
  pipeDesc.VS.BytecodeLength = TextVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = TextVS->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = TextPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = TextPS->GetBufferPointer();
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;

  hr = wrapper->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                            (void **)&Pipe[BGRA8_BACKBUFFER]);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create BGRA8 Pipe! HRESULT: %s", ToStr(hr).c_str());

  rm->SetInternalResource(Pipe[BGRA8_BACKBUFFER]);

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

  hr = wrapper->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                            (void **)&Pipe[RGBA8_BACKBUFFER]);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create RGBA8 Pipe! HRESULT: %s", ToStr(hr).c_str());

  rm->SetInternalResource(Pipe[RGBA8_BACKBUFFER]);

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

  hr = wrapper->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                            (void **)&Pipe[RGBA16_BACKBUFFER]);
  wrapper->InternalRef();

  if(FAILED(hr))
    RDCERR("Couldn't create RGBA16 Pipe! HRESULT: %s", ToStr(hr).c_str());

  rm->SetInternalResource(Pipe[RGBA16_BACKBUFFER]);

  SAFE_RELEASE(TextVS);
  SAFE_RELEASE(TextPS);
}

D3D12TextRenderer::~D3D12TextRenderer()
{
  SAFE_RELEASE(Tex);
  for(ID3D12PipelineState *pipe : Pipe)
    SAFE_RELEASE(pipe);
  SAFE_RELEASE(RootSig);
  SAFE_RELEASE(Constants);
  SAFE_RELEASE(GlyphData);
  SAFE_RELEASE(CharBuffer);
  SAFE_RELEASE(descHeap);
}

void D3D12TextRenderer::RenderText(ID3D12GraphicsCommandList *list, float x, float y,
                                   const char *textfmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, textfmt);
  StringFormat::vsnprintf(tmpBuf, 4095, textfmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderTextInternal(list, x, y, tmpBuf);
}

void D3D12TextRenderer::RenderTextInternal(ID3D12GraphicsCommandList *list, float x, float y,
                                           const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderTextInternal(list, x, y, text);
    RenderTextInternal(list, x, y + 1.0f, t + 1);
    *t = '\n';
    return;
  }

  if(strlen(text) == 0)
    return;

  RDCASSERT(strlen(text) < FONT_MAX_CHARS);

  FontCBuffer data = {};

  data.TextPosition.x = x;
  data.TextPosition.y = y;

  data.FontScreenAspect.x = 1.0f / float(GetWidth());
  data.FontScreenAspect.y = 1.0f / float(GetHeight());

  data.TextSize = CharSize;
  data.FontScreenAspect.x *= CharAspect;

  data.CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
  data.CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

  // Is 256 byte alignment on buffer offsets is just fixed, or device-specific?
  const UINT constantAlignment = 256;

  // won't read anything, empty range
  D3D12_RANGE range = {0, 0};
  byte *ptr = NULL;
  HRESULT hr = Constants->Map(0, &range, (void **)&ptr);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    memcpy(ptr + ConstRingIdx * constantAlignment, &data, sizeof(FontCBuffer));
    range.Begin = ConstRingIdx * constantAlignment;
    range.End = range.Begin + sizeof(FontCBuffer);
    Constants->Unmap(0, &range);
  }

  size_t chars = strlen(text);

  size_t charOffset = CharOffset;

  if(CharOffset + chars >= FONT_BUFFER_CHARS)
    charOffset = 0;

  CharOffset = charOffset + chars;

  CharOffset = AlignUp(CharOffset, constantAlignment / sizeof(Vec4f));

  unsigned long *texs = NULL;
  hr = CharBuffer->Map(0, NULL, (void **)&texs);

  if(FAILED(hr) || texs == NULL)
  {
    RDCERR("Failed to map charbuffer HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  texs += charOffset * 4;

  for(size_t i = 0; i < strlen(text); i++)
    texs[i * 4] = (text[i] - ' ');

  CharBuffer->Unmap(0, NULL);

  {
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    D3D12_VIEWPORT view;
    view.TopLeftX = 0;
    view.TopLeftY = 0;
    view.Width = (float)GetWidth();
    view.Height = (float)GetHeight();
    view.MinDepth = 0.0f;
    view.MaxDepth = 1.0f;
    list->RSSetViewports(1, &view);

    D3D12_RECT scissor = {0, 0, GetWidth(), GetHeight()};
    list->RSSetScissorRects(1, &scissor);

    list->SetPipelineState(Pipe[m_BBFmtIdx]);
    list->SetGraphicsRootSignature(RootSig);

    // Set the descriptor heap containing the texture srv
    list->SetDescriptorHeaps(1, &descHeap);

    list->SetGraphicsRootConstantBufferView(
        0, Constants->GetGPUVirtualAddress() + ConstRingIdx * constantAlignment);
    list->SetGraphicsRootConstantBufferView(1, GlyphData->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(
        2, CharBuffer->GetGPUVirtualAddress() + charOffset * sizeof(Vec4f));
    list->SetGraphicsRootDescriptorTable(3, descHeap->GetGPUDescriptorHandleForHeapStart());

    list->DrawInstanced(4, (uint32_t)strlen(text), 0, 0);
  }

  ConstRingIdx++;
  ConstRingIdx %= ConstRingSize;
}
