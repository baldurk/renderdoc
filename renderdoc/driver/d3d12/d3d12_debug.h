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
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"

class WrappedID3D12Device;
class D3D12ResourceManager;

class D3D12DebugManager
{
public:
  D3D12DebugManager(WrappedID3D12Device *wrapper);

  ~D3D12DebugManager();

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColour(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void SetOutputDimensions(int w, int h, DXGI_FORMAT fmt)
  {
    m_width = w;
    m_height = h;

    if(fmt == DXGI_FORMAT_B8G8R8A8_UNORM)
      m_BBFmtIdx = BGRA8_BACKBUFFER;
    else if(fmt == DXGI_FORMAT_R16G16B16A16_FLOAT)
      m_BBFmtIdx = RGBA16_BACKBUFFER;
    else if(fmt == DXGI_FORMAT_R32G32B32A32_FLOAT)
      m_BBFmtIdx = RGBA32_BACKBUFFER;
    else
      m_BBFmtIdx = RGBA8_BACKBUFFER;
  }
  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }
  void RenderText(ID3D12GraphicsCommandList *list, float x, float y, const char *textfmt, ...);

  void RenderCheckerboard(Vec3f light, Vec3f dark);
  bool RenderTexture(TextureDisplay cfg, bool blendAlpha);

  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, FormatComponentType typeHint, float pixel[4]);

  void FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, bool flattenVec4s,
                            const vector<byte> &data);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, vector<byte> &retData);
  void GetBufferData(ID3D12Resource *buff, uint64_t offset, uint64_t length, vector<byte> &retData);

  D3D12_CPU_DESCRIPTOR_HANDLE AllocRTV();
  void FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle);

  static D3D12RootSignature GetRootSig(const void *data, size_t dataSize);

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

  ID3D12Resource *MakeCBuffer(UINT64 size);
  void FillBuffer(ID3D12Resource *buf, void *data, size_t size);

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  // how much character space is in the ring buffer
  static const int FONT_BUFFER_CHARS = 8192;

  // index in SRV heap
  static const int FONT_SRV = 128;

  enum
  {
    FIXED_RTV_PICK_PIXEL,
    FIXED_RTV_CUSTOM_SHADER,
    FIXED_RTV_COUNT,
  };

  // indices for the pipelines, for the three possible backbuffer formats
  enum
  {
    BGRA8_BACKBUFFER = 0,
    RGBA8_BACKBUFFER,
    RGBA16_BACKBUFFER,
    RGBA32_BACKBUFFER,
    FMTNUM_BACKBUFFER,
  } m_BBFmtIdx;

  struct FontData
  {
    FontData() { RDCEraseMem(this, sizeof(FontData)); }
    ~FontData()
    {
      SAFE_RELEASE(Tex);
      SAFE_RELEASE(RootSig);
      SAFE_RELEASE(GlyphData);
      SAFE_RELEASE(CharBuffer);

      for(size_t i = 0; i < ARRAY_COUNT(Constants); i++)
        SAFE_RELEASE(Constants[i]);
      for(int i = 0; i < ARRAY_COUNT(Pipe); i++)
        SAFE_RELEASE(Pipe[i]);
    }

    ID3D12Resource *Tex;
    ID3D12PipelineState *Pipe[FMTNUM_BACKBUFFER];
    ID3D12RootSignature *RootSig;
    ID3D12Resource *Constants[20];
    ID3D12Resource *GlyphData;
    ID3D12Resource *CharBuffer;

    size_t CharOffset;
    size_t ConstRingIdx;

    float CharAspect;
    float CharSize;
  } m_Font;

  ID3D12DescriptorHeap *cbvsrvHeap;
  ID3D12DescriptorHeap *samplerHeap;
  ID3D12DescriptorHeap *rtvHeap;
  ID3D12DescriptorHeap *dsvHeap;

  ID3D12Resource *m_GenericVSCbuffer;
  ID3D12Resource *m_GenericPSCbuffer;

  ID3D12PipelineState *m_TexDisplayPipe;
  ID3D12PipelineState *m_TexDisplayF32Pipe;
  ID3D12PipelineState *m_TexDisplayBlendPipe;

  ID3D12RootSignature *m_TexDisplayRootSig;

  ID3D12PipelineState *m_CheckerboardPipe;

  ID3D12Resource *m_PickPixelTex;
  D3D12_CPU_DESCRIPTOR_HANDLE m_PickPixelRTV;

  ID3D12Resource *m_ReadbackBuffer;

  static const uint64_t m_ReadbackSize = 16 * 1024 * 1024;

  static const uint32_t m_ShaderCacheMagic = 0xbaafd1d1;
  static const uint32_t m_ShaderCacheVersion = 1;

  bool m_ShaderCacheDirty, m_CacheShaders;
  map<uint32_t, ID3DBlob *> m_ShaderCache;

  void FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                            const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  void RenderTextInternal(ID3D12GraphicsCommandList *list, float x, float y, const char *text);
  bool RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg, bool blendAlpha);

  string GetShaderBlob(const char *source, const char *entry, const uint32_t compileFlags,
                       const char *profile, ID3DBlob **srcblob);
  static ID3DBlob *MakeRootSig(const vector<D3D12_ROOT_PARAMETER> &rootSig);
  int m_width, m_height;

  uint64_t m_OutputWindowID;
  uint64_t m_CurrentOutputWindow;
  map<uint64_t, OutputWindow> m_OutputWindows;

  WrappedID3D12Device *m_WrappedDevice;
  ID3D12Device *m_Device;

  IDXGIFactory4 *m_pFactory;

  D3D12ResourceManager *m_ResourceManager;
};
