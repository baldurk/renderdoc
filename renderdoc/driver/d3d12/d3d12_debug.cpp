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
#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "serialise/string_utils.h"
#include "stb/stb_truetype.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"

#include "data/hlsl/debugcbuffers.h"

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);

struct D3D12BlobShaderCallbacks
{
  D3D12BlobShaderCallbacks()
  {
    HMODULE d3dcompiler = GetD3DCompiler();

    if(d3dcompiler == NULL)
      RDCFATAL("Can't get handle to d3dcompiler_??.dll");

    m_BlobCreate = (pD3DCreateBlob)GetProcAddress(d3dcompiler, "D3DCreateBlob");

    if(m_BlobCreate == NULL)
      RDCFATAL("d3dcompiler.dll doesn't contain D3DCreateBlob");
  }

  bool Create(uint32_t size, byte *data, ID3DBlob **ret) const
  {
    RDCASSERT(ret);

    *ret = NULL;
    HRESULT hr = m_BlobCreate((SIZE_T)size, ret);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create blob of size %u from shadercache: %08x", size, hr);
      return false;
    }

    memcpy((*ret)->GetBufferPointer(), data, size);

    return true;
  }

  void Destroy(ID3DBlob *blob) const { blob->Release(); }
  uint32_t GetSize(ID3DBlob *blob) const { return (uint32_t)blob->GetBufferSize(); }
  byte *GetData(ID3DBlob *blob) const { return (byte *)blob->GetBufferPointer(); }
  pD3DCreateBlob m_BlobCreate;
} ShaderCache12Callbacks;

extern "C" __declspec(dllexport) HRESULT
    __cdecl RENDERDOC_CreateWrappedDXGIFactory1(REFIID riid, void **ppFactory);

D3D12DebugManager::D3D12DebugManager(WrappedID3D12Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D12DebugManager));

  m_Device = wrapper->GetReal();
  m_ResourceManager = wrapper->GetResourceManager();

  m_OutputWindowID = 1;

  m_WrappedDevice = wrapper;
  m_WrappedDevice->InternalRef();

  m_width = m_height = 1;
  m_BBFmtIdx = BGRA8_BACKBUFFER;

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.0f);

  m_pFactory = NULL;

  HRESULT hr = S_OK;

  hr = RENDERDOC_CreateWrappedDXGIFactory1(__uuidof(IDXGIFactory4), (void **)&m_pFactory);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DXGI factory! 0x%08x", hr);
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
    RDCERR("Couldn't create RTV descriptor heap! 0x%08x", hr);
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&dsvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DSV descriptor heap! 0x%08x", hr);
  }

  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  desc.NumDescriptors = 4096;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&cbvsrvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! 0x%08x", hr);
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&samplerHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create sampler descriptor heap! 0x%08x", hr);
  }

  {
    D3D12_RESOURCE_DESC pickPixelDesc = {};
    pickPixelDesc.Alignment = 0;
    pickPixelDesc.DepthOrArraySize = 1;
    pickPixelDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    pickPixelDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    pickPixelDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    pickPixelDesc.Height = 1;
    pickPixelDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    pickPixelDesc.MipLevels = 1;
    pickPixelDesc.SampleDesc.Count = 1;
    pickPixelDesc.SampleDesc.Quality = 0;
    pickPixelDesc.Width = 1;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &pickPixelDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&m_PickPixelTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create rendering texture for pixel picking, HRESULT: 0x%08x", hr);
      return;
    }

    m_PickPixelRTV = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_PickPixelRTV.ptr +=
        FIXED_RTV_PICK_PIXEL *
        m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    m_WrappedDevice->CreateRenderTargetView(m_PickPixelTex, NULL, m_PickPixelRTV);
  }

  {
    D3D12_RESOURCE_DESC readbackDesc;
    readbackDesc.Alignment = 0;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
    readbackDesc.Height = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    readbackDesc.MipLevels = 1;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.SampleDesc.Quality = 0;
    readbackDesc.Width = m_ReadbackSize;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
        __uuidof(ID3D12Resource), (void **)&m_ReadbackBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback buffer, HRESULT: 0x%08x", hr);
      return;
    }
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.2f);

  // create fixed samplers, point and linear
  D3D12_CPU_DESCRIPTOR_HANDLE samp;
  samp = samplerHeap->GetCPUDescriptorHandleForHeapStart();

  D3D12_SAMPLER_DESC sampDesc;
  RDCEraseEl(sampDesc);

  sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampDesc.MaxAnisotropy = 1;
  sampDesc.MinLOD = 0;
  sampDesc.MaxLOD = FLT_MAX;
  sampDesc.MipLODBias = 0.0f;
  sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

  m_WrappedDevice->CreateSampler(&sampDesc, samp);

  sampDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;

  samp.ptr += m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  m_WrappedDevice->CreateSampler(&sampDesc, samp);

  m_GenericVSCbuffer = MakeCBuffer(sizeof(DebugVertexCBuffer));
  m_GenericPSCbuffer = MakeCBuffer(sizeof(DebugPixelCBufferData));

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.4f);

  bool success = LoadShaderCache("d3d12shaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, ShaderCache12Callbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  m_CacheShaders = true;

  vector<D3D12_ROOT_PARAMETER> rootSig;

  D3D12_ROOT_PARAMETER param = {};

  // m_GenericVSCbuffer
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.ShaderRegister = 0;

  rootSig.push_back(param);

  // m_GenericPSCbuffer
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.Descriptor.ShaderRegister = 1;

  rootSig.push_back(param);

  D3D12_DESCRIPTOR_RANGE srvrange = {};
  srvrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvrange.BaseShaderRegister = 0;
  srvrange.NumDescriptors = 32;
  srvrange.OffsetInDescriptorsFromTableStart = 0;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &srvrange;

  // SRV
  rootSig.push_back(param);

  D3D12_DESCRIPTOR_RANGE samplerrange = {};
  samplerrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerrange.BaseShaderRegister = 0;
  samplerrange.NumDescriptors = 2;
  samplerrange.OffsetInDescriptorsFromTableStart = 0;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &samplerrange;

  // samplers
  rootSig.push_back(param);

  ID3DBlob *root = MakeRootSig(rootSig);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature),
                                            (void **)&m_TexDisplayRootSig);

  SAFE_RELEASE(root);

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.6f);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  RDCEraseEl(pipeDesc);

  string displayhlsl = GetEmbeddedResource(debugcbuffers_h);
  displayhlsl += GetEmbeddedResource(debugcommon_hlsl);
  displayhlsl += GetEmbeddedResource(debugdisplay_hlsl);

  ID3DBlob *GenericVS = NULL;
  ID3DBlob *TexDisplayPS = NULL;
  ID3DBlob *CheckerboardPS = NULL;

  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_DebugVS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0",
                &GenericVS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_TexDisplayPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &TexDisplayPS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_CheckerboardPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &CheckerboardPS);

  RDCASSERT(GenericVS);
  RDCASSERT(TexDisplayPS);
  RDCASSERT(CheckerboardPS);

  pipeDesc.pRootSignature = m_TexDisplayRootSig;
  pipeDesc.VS.BytecodeLength = GenericVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = GenericVS->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = TexDisplayPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = TexDisplayPS->GetBufferPointer();
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayBlendPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayBlendPipe! 0x%08x", hr);
  }

  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayPipe! 0x%08x", hr);
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayF32Pipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayF32Pipe! 0x%08x", hr);
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  pipeDesc.PS.BytecodeLength = CheckerboardPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = CheckerboardPS->GetBufferPointer();

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_CheckerboardPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_CheckerboardPipe! 0x%08x", hr);
  }

  SAFE_RELEASE(GenericVS);
  SAFE_RELEASE(TexDisplayPS);
  SAFE_RELEASE(CheckerboardPS);

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.8f);

  // font rendering
  {
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

    hr = m_WrappedDevice->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&uploadBuf);

    if(FAILED(hr))
      RDCERR("Failed to create uploadBuf %08x", hr);

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

    hr = m_WrappedDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&m_Font.Tex);

    if(FAILED(hr))
      RDCERR("Failed to create m_Font.Tex %08x", hr);

    string font = GetEmbeddedResource(sourcecodepro_ttf);
    byte *ttfdata = (byte *)font.c_str();

    const int firstChar = int(' ') + 1;
    const int lastChar = 127;
    const int numChars = lastChar - firstChar;

    byte *buf = new byte[width * height];

    const float pixelHeight = 20.0f;

    stbtt_bakedchar chardata[numChars];
    stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars, chardata);

    m_Font.CharSize = pixelHeight;
    m_Font.CharAspect = chardata->xadvance / pixelHeight;

    stbtt_fontinfo f = {0};
    stbtt_InitFont(&f, ttfdata, 0);

    int ascent = 0;
    stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

    float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, pixelHeight);

    FillBuffer(uploadBuf, buf, width * height);

    delete[] buf;

    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    D3D12_TEXTURE_COPY_LOCATION dst, src;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = m_Font.Tex;
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
    barrier.Transition.pResource = m_Font.Tex;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    list->ResourceBarrier(1, &barrier);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();

    SAFE_RELEASE(uploadBuf);

    D3D12_CPU_DESCRIPTOR_HANDLE srv;
    srv = cbvsrvHeap->GetCPUDescriptorHandleForHeapStart();

    // texture display uses the first few, move to font texture slow
    srv.ptr +=
        FONT_SRV *
        m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_WrappedDevice->CreateShaderResourceView(m_Font.Tex, NULL, srv);

    Vec4f glyphData[2 * (numChars + 1)];

    m_Font.GlyphData = MakeCBuffer(sizeof(glyphData));

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

    FillBuffer(m_Font.GlyphData, &glyphData, sizeof(glyphData));

    for(size_t i = 0; i < ARRAY_COUNT(m_Font.Constants); i++)
      m_Font.Constants[i] = MakeCBuffer(sizeof(FontCBuffer));
    m_Font.CharBuffer = MakeCBuffer(FONT_BUFFER_CHARS * sizeof(uint32_t) * 4);

    m_Font.ConstRingIdx = 0;

    rootSig.clear();

    RDCEraseEl(param);

    // m_Font.Constants
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = 0;

    rootSig.push_back(param);

    // m_Font.GlyphData
    param.Descriptor.ShaderRegister = 1;
    rootSig.push_back(param);

    // CharBuffer
    param.Descriptor.ShaderRegister = 2;
    rootSig.push_back(param);

    RDCEraseEl(srvrange);
    srvrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvrange.BaseShaderRegister = 0;
    srvrange.NumDescriptors = 1;
    srvrange.OffsetInDescriptorsFromTableStart = FONT_SRV;

    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &srvrange;

    // font SRV
    rootSig.push_back(param);

    RDCEraseEl(samplerrange);
    samplerrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerrange.BaseShaderRegister = 0;
    samplerrange.NumDescriptors = 2;
    samplerrange.OffsetInDescriptorsFromTableStart = 0;

    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &samplerrange;

    // samplers
    rootSig.push_back(param);

    root = MakeRootSig(rootSig);

    RDCASSERT(root);

    hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                              __uuidof(ID3D12RootSignature),
                                              (void **)&m_Font.RootSig);

    SAFE_RELEASE(root);

    string fullhlsl = "";
    {
      string debugShaderCBuf = GetEmbeddedResource(debugcbuffers_h);
      string textShaderHLSL = GetEmbeddedResource(debugtext_hlsl);

      fullhlsl = debugShaderCBuf + textShaderHLSL;
    }

    ID3DBlob *TextVS = NULL;
    ID3DBlob *TextPS = NULL;

    GetShaderBlob(fullhlsl.c_str(), "RENDERDOC_TextVS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0",
                  &TextVS);
    GetShaderBlob(fullhlsl.c_str(), "RENDERDOC_TextPS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0",
                  &TextPS);

    RDCASSERT(TextVS);
    RDCASSERT(TextPS);

    pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;

    pipeDesc.VS.BytecodeLength = TextVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = TextVS->GetBufferPointer();
    pipeDesc.PS.BytecodeLength = TextPS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = TextPS->GetBufferPointer();

    pipeDesc.pRootSignature = m_Font.RootSig;

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[BGRA8_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create BGRA8 m_Font.Pipe! 0x%08x", hr);

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA8_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create RGBA8 m_Font.Pipe! 0x%08x", hr);

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA16_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create RGBA16 m_Font.Pipe! 0x%08x", hr);

    SAFE_RELEASE(TextVS);
    SAFE_RELEASE(TextPS);
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 1.0f);

  m_CacheShaders = false;
}

D3D12DebugManager::~D3D12DebugManager()
{
  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("d3d12shaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    ShaderCache12Callbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      ShaderCache12Callbacks.Destroy(it->second);
  }

  SAFE_RELEASE(m_pFactory);

  SAFE_RELEASE(dsvHeap);
  SAFE_RELEASE(rtvHeap);
  SAFE_RELEASE(cbvsrvHeap);
  SAFE_RELEASE(samplerHeap);

  SAFE_RELEASE(m_GenericVSCbuffer);
  SAFE_RELEASE(m_GenericPSCbuffer);

  SAFE_RELEASE(m_TexDisplayBlendPipe);
  SAFE_RELEASE(m_TexDisplayPipe);
  SAFE_RELEASE(m_TexDisplayF32Pipe);
  SAFE_RELEASE(m_TexDisplayRootSig);

  SAFE_RELEASE(m_CheckerboardPipe);

  SAFE_RELEASE(m_PickPixelTex);

  SAFE_RELEASE(m_ReadbackBuffer);

  m_WrappedDevice->InternalRelease();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

string D3D12DebugManager::GetShaderBlob(const char *source, const char *entry,
                                        const uint32_t compileFlags, const char *profile,
                                        ID3DBlob **srcblob)
{
  uint32_t hash = strhash(source);
  hash = strhash(entry, hash);
  hash = strhash(profile, hash);
  hash ^= compileFlags;

  if(m_ShaderCache.find(hash) != m_ShaderCache.end())
  {
    *srcblob = m_ShaderCache[hash];
    (*srcblob)->AddRef();
    return "";
  }

  HRESULT hr = S_OK;

  ID3DBlob *byteBlob = NULL;
  ID3DBlob *errBlob = NULL;

  HMODULE d3dcompiler = GetD3DCompiler();

  if(d3dcompiler == NULL)
  {
    RDCFATAL("Can't get handle to d3dcompiler_??.dll");
  }

  pD3DCompile compileFunc = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");

  if(compileFunc == NULL)
  {
    RDCFATAL("Can't get D3DCompile from d3dcompiler_??.dll");
  }

  uint32_t flags = compileFlags & ~D3DCOMPILE_NO_PRESHADER;

  hr = compileFunc(source, strlen(source), entry, NULL, NULL, entry, profile, flags, 0, &byteBlob,
                   &errBlob);

  string errors = "";

  if(errBlob)
  {
    errors = (char *)errBlob->GetBufferPointer();

    string logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCWARN("Shader compile error in '%s':\n%s", entry, logerror.c_str());

    SAFE_RELEASE(errBlob);

    if(FAILED(hr))
    {
      SAFE_RELEASE(byteBlob);
      return errors;
    }
  }

  if(m_CacheShaders)
  {
    m_ShaderCache[hash] = byteBlob;
    byteBlob->AddRef();
    m_ShaderCacheDirty = true;
  }

  SAFE_RELEASE(errBlob);

  *srcblob = byteBlob;
  return errors;
}

D3D12RootSignature D3D12DebugManager::GetRootSig(const void *data, size_t dataSize)
{
  PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER deserializeRootSig =
      (PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12CreateRootSignatureDeserializer");

  if(deserializeRootSig == NULL)
  {
    RDCERR("Can't get D3D12CreateRootSignatureDeserializer");
    return D3D12RootSignature();
  }

  ID3D12RootSignatureDeserializer *deser = NULL;
  HRESULT hr =
      deserializeRootSig(data, dataSize, __uuidof(ID3D12RootSignatureDeserializer), (void **)&deser);

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get deserializer");
    return D3D12RootSignature();
  }

  D3D12RootSignature ret;

  const D3D12_ROOT_SIGNATURE_DESC *desc = deser->GetRootSignatureDesc();

  ret.params.resize(desc->NumParameters);

  for(size_t i = 0; i < ret.params.size(); i++)
    ret.params[i].MakeFrom(desc->pParameters[i], ret.numSpaces);

  if(desc->NumStaticSamplers > 0)
  {
    ret.samplers.assign(desc->pStaticSamplers, desc->pStaticSamplers + desc->NumStaticSamplers);

    for(size_t i = 0; i < ret.samplers.size(); i++)
      ret.numSpaces = RDCMAX(ret.numSpaces, ret.samplers[i].RegisterSpace + 1);
  }

  SAFE_RELEASE(deser);

  return ret;
}

ID3DBlob *D3D12DebugManager::MakeRootSig(const vector<D3D12_ROOT_PARAMETER> &rootSig)
{
  PFN_D3D12_SERIALIZE_ROOT_SIGNATURE serializeRootSig =
      (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(GetModuleHandleA("d3d12.dll"),
                                                         "D3D12SerializeRootSignature");

  if(serializeRootSig == NULL)
  {
    RDCERR("Can't get D3D12SerializeRootSignature");
    return NULL;
  }

  D3D12_ROOT_SIGNATURE_DESC desc;
  desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  desc.NumStaticSamplers = 0;
  desc.pStaticSamplers = NULL;
  desc.NumParameters = (UINT)rootSig.size();
  desc.pParameters = &rootSig[0];

  ID3DBlob *ret = NULL;
  ID3DBlob *errBlob = NULL;
  HRESULT hr = serializeRootSig(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &ret, &errBlob);

  if(FAILED(hr))
  {
    string errors = (char *)errBlob->GetBufferPointer();

    string logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCERR("Root signature serialize error:\n%s", logerror.c_str());

    SAFE_RELEASE(errBlob);
    SAFE_RELEASE(ret);
    return NULL;
  }

  SAFE_RELEASE(errBlob);

  return ret;
}

ID3D12Resource *D3D12DebugManager::MakeCBuffer(UINT64 size)
{
  ID3D12Resource *ret;

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
  cbDesc.Width = size;

  HRESULT hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                        __uuidof(ID3D12Resource), (void **)&ret);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create cbuffer size %llu! 0x%08x", size, hr);
    SAFE_RELEASE(ret);
    return NULL;
  }

  return ret;
}

void D3D12DebugManager::OutputWindow::MakeRTV(bool multisampled)
{
  SAFE_RELEASE(col);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  texDesc.SampleDesc.Count = multisampled ? 1 : 1;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                            __uuidof(ID3D12Resource), (void **)&col);

  if(FAILED(hr))
  {
    RDCERR("Failed to create colour texture for window, HRESULT: 0x%08x", hr);
    return;
  }

  dev->CreateRenderTargetView(col, NULL, rtv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RTV for main window, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

void D3D12DebugManager::OutputWindow::MakeDSV()
{
  SAFE_RELEASE(depth);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.SampleDesc.Count = 1;
  texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV texture for output window, HRESULT: 0x%08x", hr);
    return;
  }

  dev->CreateDepthStencilView(depth, NULL, dsv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV for output window, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

uint64_t D3D12DebugManager::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  RDCASSERT(system == eWindowingSystem_Win32, system);

  OutputWindow outw;
  outw.wnd = (HWND)data;
  outw.dev = m_WrappedDevice;

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

  hr = m_pFactory->CreateSwapChain(m_WrappedDevice->GetQueue(), &swapDesc, &outw.swap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create swap chain for HWND, HRESULT: 0x%08x", hr);
    return 0;
  }

  outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb[0]);
  outw.swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&outw.bb[1]);

  outw.bbIdx = 0;

  outw.rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  outw.rtv.ptr += FIXED_RTV_COUNT *
                  m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  outw.rtv.ptr += SIZE_T(m_OutputWindowID) *
                  m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  outw.dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
  outw.dsv.ptr += SIZE_T(m_OutputWindowID) *
                  m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  outw.col = NULL;
  outw.MakeRTV(depth);
  m_WrappedDevice->CreateRenderTargetView(outw.col, NULL, outw.rtv);

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
  SAFE_RELEASE(outw.bb[0]);
  SAFE_RELEASE(outw.bb[1]);
  SAFE_RELEASE(outw.col);
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

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists(true);

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
        RDCERR("Failed to resize swap chain, HRESULT: 0x%08x", hr);
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

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->ClearRenderTargetView(m_OutputWindows[id].rtv, col, 0, NULL);

  list->Close();
}

void D3D12DebugManager::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->ClearDepthStencilView(m_OutputWindows[id].dsv,
                              D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0,
                              NULL);

  list->Close();
}

void D3D12DebugManager::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  m_CurrentOutputWindow = id;

  if(outw.bb[0] == NULL)
    return;

  SetOutputDimensions(outw.width, outw.height, DXGI_FORMAT_UNKNOWN);
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

  OutputWindow &outw = m_OutputWindows[id];

  if(m_OutputWindows[id].bb[0] == NULL)
    return;

  D3D12_RESOURCE_BARRIER barriers[2];
  RDCEraseEl(barriers);

  barriers[0].Transition.pResource = outw.col;
  barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  barriers[1].Transition.pResource = outw.bb[outw.bbIdx];
  barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  // transition colour to copy source, backbuffer to copy test
  list->ResourceBarrier(2, barriers);

  // resolve or copy from colour to backbuffer
  /*
  if(outw.depth)
    list->ResolveSubresource(barriers[1].Transition.pResource, 0, barriers[0].Transition.pResource,
                             0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
  else
  */
  list->CopyResource(barriers[1].Transition.pResource, barriers[0].Transition.pResource);

  std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
  std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);

  // transition colour back to render target, and backbuffer back to present
  list->ResourceBarrier(2, barriers);

  list->Close();

  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();

  outw.swap->Present(0, 0);

  outw.bbIdx++;
  outw.bbIdx %= 2;
}

void D3D12DebugManager::FillBuffer(ID3D12Resource *buf, void *data, size_t size)
{
  void *ptr = NULL;
  HRESULT hr = buf->Map(0, NULL, &ptr);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer %08x", hr);
  }
  else
  {
    memcpy(ptr, data, size);
    buf->Unmap(0, NULL);
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::AllocRTV()
{
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtv.ptr += SIZE_T(m_OutputWindowID) *
             m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  m_OutputWindowID++;

  return rtv;
}

void D3D12DebugManager::FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  // do nothing for now but could recycle/free-list/etc RTVs
  D3D12NOTIMP("Not freeing RTV's - will run out");
}

void D3D12DebugManager::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                                  uint32_t mip, uint32_t sample, FormatComponentType typeHint,
                                  float pixel[4])
{
  int oldW = GetWidth(), oldH = GetHeight();

  SetOutputDimensions(1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

  {
    TextureDisplay texDisplay;

    texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
    texDisplay.HDRMul = -1.0f;
    texDisplay.linearDisplayAsGamma = true;
    texDisplay.FlipY = false;
    texDisplay.mip = mip;
    texDisplay.sampleIdx = sample;
    texDisplay.CustomShader = ResourceId();
    texDisplay.sliceFace = sliceFace;
    texDisplay.rangemin = 0.0f;
    texDisplay.rangemax = 1.0f;
    texDisplay.scale = 1.0f;
    texDisplay.texid = texture;
    texDisplay.typeHint = typeHint;
    texDisplay.rawoutput = true;
    texDisplay.offx = -float(x);
    texDisplay.offy = -float(y);

    RenderTextureInternal(m_PickPixelRTV, texDisplay, false);
  }

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = m_PickPixelTex;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  list->ResourceBarrier(1, &barrier);

  D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};

  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.pResource = m_PickPixelTex;
  src.SubresourceIndex = 0;

  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.pResource = m_ReadbackBuffer;
  dst.PlacedFootprint.Offset = 0;
  dst.PlacedFootprint.Footprint.Width = sizeof(Vec4f);
  dst.PlacedFootprint.Footprint.Height = 1;
  dst.PlacedFootprint.Footprint.Depth = 1;
  dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  dst.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

  list->ResourceBarrier(1, &barrier);

  list->Close();

  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();

  D3D12_RANGE range = {0, sizeof(Vec4f)};

  float *pix = NULL;
  HRESULT hr = m_ReadbackBuffer->Map(0, &range, (void **)&pix);

  if(FAILED(hr))
  {
    RDCERR("Failed to map picking stage tex %08x", hr);
  }

  if(pix == NULL)
  {
    RDCERR("Failed to map pick-pixel staging texture.");
  }
  else
  {
    pixel[0] = pix[0];
    pixel[1] = pix[1];
    pixel[2] = pix[2];
    pixel[3] = pix[3];
  }

  SetOutputDimensions(oldW, oldH, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

  range.End = 0;

  if(SUCCEEDED(hr))
    m_ReadbackBuffer->Unmap(0, &range);
}

void D3D12DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                                             const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars,
                                             const vector<byte> &data)
{
  using namespace DXBC;
  using namespace ShaderDebug;

  size_t o = offset;

  for(size_t v = 0; v < invars.size(); v++)
  {
    size_t vec = o + invars[v].descriptor.offset / 16;
    size_t comp = (invars[v].descriptor.offset - (invars[v].descriptor.offset & ~0xf)) / 4;
    size_t sz = RDCMAX(1U, invars[v].type.descriptor.bytesize / 16);

    offset = vec + sz;

    string basename = prefix + invars[v].name;

    uint32_t rows = invars[v].type.descriptor.rows;
    uint32_t cols = invars[v].type.descriptor.cols;
    uint32_t elems = RDCMAX(1U, invars[v].type.descriptor.elements);

    if(!invars[v].type.members.empty())
    {
      char buf[64] = {0};
      StringFormat::snprintf(buf, 63, "[%d]", elems);

      ShaderVariable var;
      var.name = basename;
      var.rows = var.columns = 0;
      var.type = eVar_Float;

      std::vector<ShaderVariable> varmembers;

      if(elems > 1)
      {
        for(uint32_t i = 0; i < elems; i++)
        {
          StringFormat::snprintf(buf, 63, "[%d]", i);

          if(flatten)
          {
            FillCBufferVariables(basename + buf + ".", vec, flatten, invars[v].type.members,
                                 outvars, data);
          }
          else
          {
            ShaderVariable vr;
            vr.name = basename + buf;
            vr.rows = vr.columns = 0;
            vr.type = eVar_Float;

            std::vector<ShaderVariable> mems;

            FillCBufferVariables("", vec, flatten, invars[v].type.members, mems, data);

            vr.isStruct = true;

            vr.members = mems;

            varmembers.push_back(vr);
          }
        }

        var.isStruct = false;
      }
      else
      {
        var.isStruct = true;

        if(flatten)
          FillCBufferVariables(basename + ".", vec, flatten, invars[v].type.members, outvars, data);
        else
          FillCBufferVariables("", vec, flatten, invars[v].type.members, varmembers, data);
      }

      if(!flatten)
      {
        var.members = varmembers;
        outvars.push_back(var);
      }

      continue;
    }

    if(invars[v].type.descriptor.varClass == CLASS_OBJECT ||
       invars[v].type.descriptor.varClass == CLASS_STRUCT ||
       invars[v].type.descriptor.varClass == CLASS_INTERFACE_CLASS ||
       invars[v].type.descriptor.varClass == CLASS_INTERFACE_POINTER)
    {
      RDCWARN("Unexpected variable '%s' of class '%u' in cbuffer, skipping.",
              invars[v].name.c_str(), invars[v].type.descriptor.type);
      continue;
    }

    size_t elemByteSize = 4;
    VarType type = eVar_Float;
    switch(invars[v].type.descriptor.type)
    {
      case VARTYPE_INT: type = eVar_Int; break;
      case VARTYPE_FLOAT: type = eVar_Float; break;
      case VARTYPE_BOOL:
      case VARTYPE_UINT:
      case VARTYPE_UINT8: type = eVar_UInt; break;
      case VARTYPE_DOUBLE:
        elemByteSize = 8;
        type = eVar_Double;
        break;
      default:
        RDCERR("Unexpected type %d for variable '%s' in cbuffer", invars[v].type.descriptor.type,
               invars[v].name.c_str());
    }

    bool columnMajor = invars[v].type.descriptor.varClass == CLASS_MATRIX_COLUMNS;

    size_t outIdx = vec;
    if(!flatten)
    {
      outIdx = outvars.size();
      outvars.resize(RDCMAX(outIdx + 1, outvars.size()));
    }
    else
    {
      if(columnMajor)
        outvars.resize(RDCMAX(outIdx + cols * elems, outvars.size()));
      else
        outvars.resize(RDCMAX(outIdx + rows * elems, outvars.size()));
    }

    size_t dataOffset = vec * sizeof(Vec4f) + comp * sizeof(float);

    if(outvars[outIdx].name.count > 0)
    {
      RDCASSERT(flatten);

      RDCASSERT(outvars[vec].rows == 1);
      RDCASSERT(outvars[vec].columns == comp);
      RDCASSERT(rows == 1);

      string combinedName = outvars[outIdx].name.elems;
      combinedName += ", " + basename;
      outvars[outIdx].name = combinedName;
      outvars[outIdx].rows = 1;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns += cols;

      if(dataOffset < data.size())
      {
        const byte *d = &data[dataOffset];

        memcpy(&outvars[outIdx].value.uv[comp], d,
               RDCMIN(data.size() - dataOffset, elemByteSize * cols));
      }
    }
    else
    {
      outvars[outIdx].name = basename;
      outvars[outIdx].rows = 1;
      outvars[outIdx].type = type;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns = cols;

      ShaderVariable &var = outvars[outIdx];

      bool isArray = invars[v].type.descriptor.elements > 1;

      if(rows * elems == 1)
      {
        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          memcpy(&outvars[outIdx].value.uv[flatten ? comp : 0], d,
                 RDCMIN(data.size() - dataOffset, elemByteSize * cols));
        }
      }
      else if(!isArray && !flatten)
      {
        outvars[outIdx].rows = rows;

        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          RDCASSERT(rows <= 4 && rows * cols <= 16);

          if(columnMajor)
          {
            uint32_t tmp[16] = {0};

            // matrices always have 4 columns, for padding reasons (the same reason arrays
            // put every element on a new vec4)
            for(uint32_t c = 0; c < cols; c++)
            {
              size_t srcoffs = 4 * elemByteSize * c;
              size_t dstoffs = rows * elemByteSize * c;
              memcpy((byte *)(tmp) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * rows));
            }

            // transpose
            for(size_t r = 0; r < rows; r++)
              for(size_t c = 0; c < cols; c++)
                outvars[outIdx].value.uv[r * cols + c] = tmp[c * rows + r];
          }
          else    // CLASS_MATRIX_ROWS or other data not to transpose.
          {
            // matrices always have 4 columns, for padding reasons (the same reason arrays
            // put every element on a new vec4)
            for(uint32_t r = 0; r < rows; r++)
            {
              size_t srcoffs = 4 * elemByteSize * r;
              size_t dstoffs = cols * elemByteSize * r;
              memcpy((byte *)(&outvars[outIdx].value.uv[0]) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * cols));
            }
          }
        }
      }
      else if(rows * elems > 1)
      {
        char buf[64] = {0};

        var.name = outvars[outIdx].name;

        vector<ShaderVariable> varmembers;
        vector<ShaderVariable> *out = &outvars;
        size_t rowCopy = 1;

        uint32_t registers = rows;
        uint32_t regLen = cols;
        const char *regName = "row";

        string base = outvars[outIdx].name.elems;

        if(!flatten)
        {
          var.rows = 0;
          var.columns = 0;
          outIdx = 0;
          out = &varmembers;
          varmembers.resize(elems);
          rowCopy = rows;
          rows = 1;
          registers = 1;
        }
        else
        {
          if(columnMajor)
          {
            registers = cols;
            regLen = rows;
            regName = "col";
          }
        }

        size_t rowDataOffset = vec * sizeof(Vec4f);

        for(size_t r = 0; r < registers * elems; r++)
        {
          if(isArray && registers > 1)
            StringFormat::snprintf(buf, 63, "[%d].%s%d", r / registers, regName, r % registers);
          else if(registers > 1)
            StringFormat::snprintf(buf, 63, ".%s%d", regName, r);
          else
            StringFormat::snprintf(buf, 63, "[%d]", r);

          (*out)[outIdx + r].name = base + buf;
          (*out)[outIdx + r].rows = (uint32_t)rowCopy;
          (*out)[outIdx + r].type = type;
          (*out)[outIdx + r].isStruct = false;
          (*out)[outIdx + r].columns = regLen;

          size_t totalSize = 0;

          if(flatten)
          {
            totalSize = elemByteSize * regLen;
          }
          else
          {
            // in a matrix, each major element before the last takes up a full
            // vec4 at least
            size_t vecSize = elemByteSize * 4;

            if(columnMajor)
              totalSize = vecSize * (cols - 1) + elemByteSize * rowCopy;
            else
              totalSize = vecSize * (rowCopy - 1) + elemByteSize * cols;
          }

          if((rowDataOffset % sizeof(Vec4f) != 0) &&
             (rowDataOffset / sizeof(Vec4f) != (rowDataOffset + totalSize) / sizeof(Vec4f)))
          {
            rowDataOffset = AlignUp(rowDataOffset, sizeof(Vec4f));
          }

          if(rowDataOffset < data.size())
          {
            const byte *d = &data[rowDataOffset];

            memcpy(&((*out)[outIdx + r].value.uv[0]), d,
                   RDCMIN(data.size() - rowDataOffset, totalSize));

            if(!flatten && columnMajor)
            {
              ShaderVariable tmp = (*out)[outIdx + r];

              size_t transposeRows = rowCopy > 1 ? 4 : 1;

              // transpose
              for(size_t ri = 0; ri < transposeRows; ri++)
                for(size_t ci = 0; ci < cols; ci++)
                  (*out)[outIdx + r].value.uv[ri * cols + ci] = tmp.value.uv[ci * transposeRows + ri];
            }
          }

          if(flatten)
          {
            rowDataOffset += sizeof(Vec4f);
          }
          else
          {
            if(columnMajor)
              rowDataOffset += sizeof(Vec4f) * (cols - 1) + sizeof(float) * rowCopy;
            else
              rowDataOffset += sizeof(Vec4f) * (rowCopy - 1) + sizeof(float) * cols;
          }
        }

        if(!flatten)
        {
          var.isStruct = false;
          var.members = varmembers;
        }
      }
    }
  }
}

void D3D12DebugManager::FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars, bool flattenVec4s,
                                             const vector<byte> &data)
{
  size_t zero = 0;

  vector<ShaderVariable> v;
  FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

  outvars.reserve(v.size());
  for(size_t i = 0; i < v.size(); i++)
    outvars.push_back(v[i]);
}

void D3D12DebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length,
                                      vector<byte> &retData)
{
  auto it = WrappedID3D12Resource::m_List.find(buff);

  if(it == WrappedID3D12Resource::m_List.end())
  {
    RDCERR("Getting buffer data for unknown buffer %llu!", buff);
    return;
  }

  WrappedID3D12Resource *buffer = it->second;

  RDCASSERT(buffer);

  GetBufferData(buffer, offset, length, retData);
}

void D3D12DebugManager::GetBufferData(ID3D12Resource *buffer, uint64_t offset, uint64_t length,
                                      vector<byte> &ret)
{
  if(buffer == NULL)
    return;

  D3D12_RESOURCE_DESC desc = buffer->GetDesc();
  D3D12_HEAP_PROPERTIES heapProps;
  buffer->GetHeapProperties(&heapProps, NULL);

  if(offset >= desc.Width)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(length == 0)
  {
    length = desc.Width - offset;
  }

  if(length > 0 && offset + length > desc.Width)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%llu)",
            offset, length, desc.Width);
    length = RDCMIN(length, desc.Width - offset);
  }

#ifndef RDC64BIT
  if(offset + length > 0xfffffff)
  {
    RDCERR("Trying to read back too much data on 32-bit build. Try running on 64-bit.");
    return;
  }
#endif

  uint64_t outOffs = 0;

  ret.resize((size_t)length);

  // directly CPU mappable (and possibly invalid to transition and copy from), so just memcpy
  if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD || heapProps.Type == D3D12_HEAP_TYPE_READBACK)
  {
    D3D12_RANGE range = {(size_t)offset, size_t(offset + length)};

    byte *data = NULL;
    HRESULT hr = buffer->Map(0, &range, (void **)&data);

    if(FAILED(hr))
    {
      RDCERR("Failed to map buffer directly for readback %08x", hr);
      return;
    }

    memcpy(&ret[0], data + offset, (size_t)length);

    range.Begin = range.End = 0;

    buffer->Unmap(0, &range);

    return;
  }

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = buffer;
  barrier.Transition.StateBefore = m_WrappedDevice->GetSubresourceStates(GetResID(buffer))[0];
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  list->ResourceBarrier(1, &barrier);

  while(length > 0)
  {
    uint64_t chunkSize = RDCMIN(length, m_ReadbackSize);

    list->CopyBufferRegion(m_ReadbackBuffer, 0, buffer, offset, chunkSize);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();

    D3D12_RANGE range = {0, (size_t)chunkSize};

    void *data = NULL;
    HRESULT hr = m_ReadbackBuffer->Map(0, &range, &data);

    if(FAILED(hr))
    {
      RDCERR("Failed to map bufferdata buffer %08x", hr);
      return;
    }
    else
    {
      memcpy(&ret[(size_t)outOffs], data, (size_t)chunkSize);

      range.End = 0;

      m_ReadbackBuffer->Unmap(0, &range);
    }

    outOffs += chunkSize;
    length -= chunkSize;

    if(length > 0)
      list = m_WrappedDevice->GetNewList();
  }
}

void D3D12DebugManager::RenderCheckerboard(Vec3f light, Vec3f dark)
{
  DebugVertexCBuffer vertexData;

  vertexData.Scale = 2.0f;
  vertexData.Position.x = vertexData.Position.y = 0;

  vertexData.ScreenAspect.x = 1.0f;
  vertexData.ScreenAspect.y = 1.0f;

  vertexData.TextureResolution.x = 1.0f;
  vertexData.TextureResolution.y = 1.0f;

  vertexData.LineStrip = 0;

  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  pixelData.Channels = Vec4f(light.x, light.y, light.z, 0.0f);
  pixelData.WireframeColour = dark;

  FillBuffer(m_GenericVSCbuffer, &vertexData, sizeof(DebugVertexCBuffer));
  FillBuffer(m_GenericPSCbuffer, &pixelData, sizeof(DebugPixelCBufferData));

  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    list->OMSetRenderTargets(1, &outw.rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, outw.width, outw.height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    list->SetPipelineState(m_CheckerboardPipe);

    list->SetGraphicsRootSignature(m_TexDisplayRootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    list->SetGraphicsRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, m_GenericPSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootDescriptorTable(2, cbvsrvHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(3, samplerHeap->GetGPUDescriptorHandleForHeapStart());

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(4, 1, 0, 0);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }
}

void D3D12DebugManager::RenderText(ID3D12GraphicsCommandList *list, float x, float y,
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

void D3D12DebugManager::RenderTextInternal(ID3D12GraphicsCommandList *list, float x, float y,
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

  FontCBuffer data;

  data.TextPosition.x = x;
  data.TextPosition.y = y;

  data.FontScreenAspect.x = 1.0f / float(GetWidth());
  data.FontScreenAspect.y = 1.0f / float(GetHeight());

  data.TextSize = m_Font.CharSize;
  data.FontScreenAspect.x *= m_Font.CharAspect;

  data.CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
  data.CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

  FillBuffer(m_Font.Constants[m_Font.ConstRingIdx], &data, sizeof(FontCBuffer));

  size_t chars = strlen(text);

  size_t charOffset = m_Font.CharOffset;

  if(m_Font.CharOffset + chars >= FONT_BUFFER_CHARS)
    charOffset = 0;

  m_Font.CharOffset = charOffset + chars;

  // Is 256 byte alignment on buffer offsets is just fixed, or device-specific?
  m_Font.CharOffset = AlignUp(m_Font.CharOffset, 256 / sizeof(Vec4f));

  unsigned long *texs = NULL;
  HRESULT hr = m_Font.CharBuffer->Map(0, NULL, (void **)&texs);

  if(FAILED(hr) || texs == NULL)
  {
    RDCERR("Failed to map charbuffer %08x", hr);
    return;
  }

  texs += charOffset * 4;

  for(size_t i = 0; i < strlen(text); i++)
    texs[i * 4] = (text[i] - ' ');

  m_Font.CharBuffer->Unmap(0, NULL);

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

    list->SetPipelineState(m_Font.Pipe[m_BBFmtIdx]);
    list->SetGraphicsRootSignature(m_Font.RootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    list->SetGraphicsRootConstantBufferView(
        0, m_Font.Constants[m_Font.ConstRingIdx]->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, m_Font.GlyphData->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(
        2, m_Font.CharBuffer->GetGPUVirtualAddress() + charOffset * sizeof(Vec4f));
    list->SetGraphicsRootDescriptorTable(3, cbvsrvHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(4, samplerHeap->GetGPUDescriptorHandleForHeapStart());

    list->DrawInstanced(4, (uint32_t)strlen(text), 0, 0);
  }

  m_Font.ConstRingIdx++;
  m_Font.ConstRingIdx %= ARRAY_COUNT(m_Font.Constants);
}

bool D3D12DebugManager::RenderTexture(TextureDisplay cfg, bool blendAlpha)
{
  return RenderTextureInternal(m_OutputWindows[m_CurrentOutputWindow].rtv, cfg, blendAlpha);
}

bool D3D12DebugManager::RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                                              bool blendAlpha)
{
  DebugVertexCBuffer vertexData;
  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  float x = cfg.offx;
  float y = cfg.offy;

  vertexData.Position.x = x * (2.0f / float(GetWidth()));
  vertexData.Position.y = -y * (2.0f / float(GetHeight()));

  vertexData.ScreenAspect.x =
      (float(GetHeight()) / float(GetWidth()));    // 0.5 = character width / character height
  vertexData.ScreenAspect.y = 1.0f;

  vertexData.TextureResolution.x = 1.0f / vertexData.ScreenAspect.x;
  vertexData.TextureResolution.y = 1.0f;

  vertexData.LineStrip = 0;

  if(cfg.rangemax <= cfg.rangemin)
    cfg.rangemax += 0.00001f;

  pixelData.Channels.x = cfg.Red ? 1.0f : 0.0f;
  pixelData.Channels.y = cfg.Green ? 1.0f : 0.0f;
  pixelData.Channels.z = cfg.Blue ? 1.0f : 0.0f;
  pixelData.Channels.w = cfg.Alpha ? 1.0f : 0.0f;

  pixelData.RangeMinimum = cfg.rangemin;
  pixelData.InverseRangeSize = 1.0f / (cfg.rangemax - cfg.rangemin);

  if(_isnan(pixelData.InverseRangeSize) || !_finite(pixelData.InverseRangeSize))
  {
    pixelData.InverseRangeSize = FLT_MAX;
  }

  pixelData.WireframeColour.x = cfg.HDRMul;

  pixelData.RawOutput = cfg.rawoutput ? 1 : 0;

  pixelData.FlipY = cfg.FlipY ? 1 : 0;

  D3D12NOTIMP("proper texture display");
  // GetShaderDetails(cfg.texid, cfg.typeHint, cfg.rawoutput ? true : false);
  WrappedID3D12Resource *resource = WrappedID3D12Resource::m_List[cfg.texid];
  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  pixelData.SampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, resourceDesc.SampleDesc.Count - 1);

  // hacky resolve
  if(cfg.sampleIdx == ~0U)
    pixelData.SampleIdx = -int(resourceDesc.SampleDesc.Count);

  if(resourceDesc.Format == DXGI_FORMAT_UNKNOWN)
    return false;

  if(resourceDesc.Format == DXGI_FORMAT_A8_UNORM && cfg.scale <= 0.0f)
  {
    pixelData.Channels.x = pixelData.Channels.y = pixelData.Channels.z = 0.0f;
    pixelData.Channels.w = 1.0f;
  }

  float tex_x = float(resourceDesc.Width);
  float tex_y =
      float(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? 100 : resourceDesc.Height);

  vertexData.TextureResolution.x *= tex_x / float(GetWidth());
  vertexData.TextureResolution.y *= tex_y / float(GetHeight());

  pixelData.TextureResolutionPS.x = float(RDCMAX(1U, uint32_t(resourceDesc.Width >> cfg.mip)));
  pixelData.TextureResolutionPS.y = float(RDCMAX(1U, uint32_t(resourceDesc.Height >> cfg.mip)));
  pixelData.TextureResolutionPS.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> cfg.mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    pixelData.TextureResolutionPS.z = float(resourceDesc.DepthOrArraySize);

  vertexData.Scale = cfg.scale;
  pixelData.ScalePS = cfg.scale;

  if(cfg.scale <= 0.0f)
  {
    float xscale = float(GetWidth()) / tex_x;
    float yscale = float(GetHeight()) / tex_y;

    vertexData.Scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      vertexData.Position.x = 0;
      vertexData.Position.y = tex_y * vertexData.Scale / float(GetHeight()) - 1.0f;
    }
    else
    {
      vertexData.Position.y = 0;
      vertexData.Position.x = 1.0f - tex_x * vertexData.Scale / float(GetWidth());
    }
  }

  vertexData.Scale *= 2.0f;    // viewport is -1 -> 1

  pixelData.MipLevel = (float)cfg.mip;
  pixelData.OutputDisplayFormat = RESTYPE_TEX2D;
  pixelData.Slice = float(RDCCLAMP(cfg.sliceFace, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX3D;
    pixelData.Slice = float(cfg.sliceFace) / float(resourceDesc.DepthOrArraySize);
  }
  else if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX1D;
  }
  /*
  else if(details.texType == eTexType_Depth)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH;
  }
  else if(details.texType == eTexType_Stencil)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH_STENCIL;
  }
  else if(details.texType == eTexType_DepthMS)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH_MS;
  }
  else if(details.texType == eTexType_StencilMS)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH_STENCIL_MS;
  }*/
  else if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
          resourceDesc.SampleDesc.Count > 1)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX2D_MS;
  }

  if(cfg.overlay == eTexOverlay_NaN)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;
  }

  if(cfg.overlay == eTexOverlay_Clipping)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;
  }

  int srvOffset = 0;

  if(IsUIntFormat(resourceDesc.Format))
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
    srvOffset = 10;
  }
  if(IsIntFormat(resourceDesc.Format))
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;
    srvOffset = 20;
  }
  if(!IsSRGBFormat(resourceDesc.Format) && cfg.linearDisplayAsGamma)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE srv = cbvsrvHeap->GetCPUDescriptorHandleForHeapStart();

  // hack, tex2d float is slot 2
  srv.ptr +=
      2 * m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Format = GetTypedFormat(resourceDesc.Format, cfg.typeHint);
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = ~0U;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.PlaneSlice = 0;
  srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

  // can't display depth textures directly yet
  if(IsDepthFormat(srvDesc.Format) && !IsTypelessFormat(srvDesc.Format))
    return true;

  // or non-2D
  if(resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    return true;

  if(resource->GetDesc().SampleDesc.Count > 1)
  {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    srv.ptr +=
        7 * m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }

  m_WrappedDevice->CreateShaderResourceView(resource, &srvDesc, srv);

  FillBuffer(m_GenericVSCbuffer, &vertexData, sizeof(DebugVertexCBuffer));
  FillBuffer(m_GenericPSCbuffer, &pixelData, sizeof(DebugPixelCBufferData));

  // transition resource to D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  const vector<D3D12_RESOURCE_STATES> &states =
      m_WrappedDevice->GetSubresourceStates(GetResID(resource));

  vector<D3D12_RESOURCE_BARRIER> barriers;
  barriers.reserve(states.size());
  for(size_t i = 0; i < states.size(); i++)
  {
    D3D12_RESOURCE_BARRIER b;

    // skip unneeded barriers
    if(states[i] & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
      continue;

    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = resource;
    b.Transition.Subresource = (UINT)i;
    b.Transition.StateBefore = states[i];
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    barriers.push_back(b);
  }

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, m_width, m_height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    if(cfg.rawoutput || !blendAlpha || cfg.CustomShader != ResourceId())
    {
      if(m_BBFmtIdx == RGBA32_BACKBUFFER)
        list->SetPipelineState(m_TexDisplayF32Pipe);
      else
        list->SetPipelineState(m_TexDisplayPipe);
    }
    else
    {
      list->SetPipelineState(m_TexDisplayBlendPipe);
    }

    list->SetGraphicsRootSignature(m_TexDisplayRootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    list->SetGraphicsRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, m_GenericPSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootDescriptorTable(2, cbvsrvHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(3, samplerHeap->GetGPUDescriptorHandleForHeapStart());

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(4, 1, 0, 0);

    // transition back to where they were
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }

  return true;
}
