/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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
#include "api/replay/renderdoc_replay.h"
#include "driver/dx/official/d3d11_4.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "replay/replay_driver.h"
#include "d3d11_renderstate.h"

using std::map;
using std::pair;

class Camera;
class Vec3f;

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;

class AMDCounters;

struct DrawcallTreeNode;

struct D3D11CounterContext;

class D3D11ResourceManager;

namespace ShaderDebug
{
struct GlobalState;
}

struct D3D11PostVSData
{
  struct InstData
  {
    uint32_t numVerts = 0;
    uint32_t bufOffset = 0;
  };

  struct StageData
  {
    ID3D11Buffer *buf = NULL;
    D3D11_PRIMITIVE_TOPOLOGY topo = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    uint32_t vertStride = 0;

    // simple case - uniform
    uint32_t numVerts = 0;
    uint32_t instStride = 0;

    // complex case - expansion per instance
    std::vector<InstData> instData;

    bool useIndices = false;
    ID3D11Buffer *idxBuf = NULL;
    DXGI_FORMAT idxFmt = DXGI_FORMAT_UNKNOWN;

    bool hasPosOut = false;

    float nearPlane = 0.0f;
    float farPlane = 0.0f;
  } vsin, vsout, gsout;

  const StageData &GetStage(MeshDataStage type)
  {
    if(type == MeshDataStage::VSOut)
      return vsout;
    else if(type == MeshDataStage::GSOut)
      return gsout;
    else
      RDCERR("Unexpected mesh data stage!");

    return vsin;
  }
};

struct CopyPixelParams;
struct GetTextureDataParams;

class D3D11DebugManager
{
public:
  D3D11DebugManager(WrappedID3D11Device *wrapper);
  ~D3D11DebugManager();

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColor(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void SetOutputDimensions(int w, int h)
  {
    m_width = w;
    m_height = h;
  }
  void SetOutputWindow(HWND w);
  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }
  void InitPostVSBuffers(uint32_t eventID);
  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  uint32_t GetStructCount(ID3D11UnorderedAccessView *uav);
  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, vector<byte> &retData);
  void GetBufferData(ID3D11Buffer *buff, uint64_t offset, uint64_t length, vector<byte> &retData);

  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, bool flattenVec4s,
                            const vector<byte> &data);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  void CopyArrayToTex2DMS(ID3D11Texture2D *destMS, ID3D11Texture2D *srcArray);
  void CopyTex2DMSToArray(ID3D11Texture2D *destArray, ID3D11Texture2D *srcMS);

  // called before any device is created, to init any counters
  static void PreDeviceInitCounters();

  // called after any device is destroyed, to do corresponding shutdown of counters
  static void PostDeviceShutdownCounters();

  vector<GPUCounter> EnumerateCounters();
  void DescribeCounter(GPUCounter counterID, CounterDescription &desc);
  vector<CounterResult> FetchCounters(const vector<GPUCounter> &counters);
  vector<CounterResult> FetchCountersAMD(const vector<GPUCounter> &counters);

  void RenderText(float x, float y, const char *textfmt, ...);
  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  ID3D11Buffer *MakeCBuffer(const void *data, size_t size);

  string GetShaderBlob(const char *source, const char *entry, const uint32_t compileFlags,
                       const char *profile, ID3DBlob **srcblob);
  ID3D11VertexShader *MakeVShader(const char *source, const char *entry, const char *profile,
                                  int numInputDescs = 0, D3D11_INPUT_ELEMENT_DESC *inputs = NULL,
                                  ID3D11InputLayout **ret = NULL, vector<byte> *blob = NULL);
  ID3D11GeometryShader *MakeGShader(const char *source, const char *entry, const char *profile);
  ID3D11PixelShader *MakePShader(const char *source, const char *entry, const char *profile);
  ID3D11ComputeShader *MakeCShader(const char *source, const char *entry, const char *profile);

  void BuildShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                   ResourceId *id, string *errors);

  ID3D11Buffer *MakeCBuffer(UINT size);

  bool RenderTexture(TextureDisplay cfg, bool blendAlpha);

  void RenderCheckerboard(Vec3f light, Vec3f dark);

  void RenderHighlightBox(float w, float h, float scale);

  vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x,
                                         uint32_t y, uint32_t slice, uint32_t mip,
                                         uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventID, const uint32_t groupid[3],
                               const uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventID, const vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  // don't need to differentiate arrays as we treat everything
  // as an array (potentially with only one element).
  enum TextureType
  {
    eTexType_1D = 1,
    eTexType_2D,
    eTexType_3D,
    eTexType_Depth,
    eTexType_Stencil,
    eTexType_DepthMS,
    eTexType_StencilMS,
    eTexType_Unused,    // removed, kept just to keep slots the same
    eTexType_2DMS,
    eTexType_Max
  };

  struct TextureShaderDetails
  {
    TextureShaderDetails()
    {
      texFmt = DXGI_FORMAT_UNKNOWN;
      texWidth = 0;
      texHeight = 0;
      texDepth = 0;
      texMips = 0;
      texArraySize = 0;

      sampleCount = 1;
      sampleQuality = 0;

      texType = eTexType_2D;

      srvResource = NULL;
      previewCopy = NULL;

      RDCEraseEl(srv);
    }

    DXGI_FORMAT texFmt;
    UINT texWidth;
    UINT texHeight;
    UINT texDepth;
    UINT texMips;
    UINT texArraySize;

    UINT sampleCount;
    UINT sampleQuality;

    TextureType texType;

    ID3D11Resource *srvResource;
    ID3D11Resource *previewCopy;

    ID3D11ShaderResourceView *srv[eTexType_Max];
  };

  TextureShaderDetails GetShaderDetails(ResourceId id, CompType typeHint, bool rawOutput);

  AMDCounters *m_pAMDCounters;

private:
  struct CacheElem
  {
    CacheElem(ResourceId id_, CompType typeHint_, bool raw_)
        : created(false), id(id_), typeHint(typeHint_), raw(raw_), srvResource(NULL)
    {
      srv[0] = srv[1] = NULL;
    }

    void Release()
    {
      SAFE_RELEASE(srvResource);
      SAFE_RELEASE(srv[0]);
      SAFE_RELEASE(srv[1]);
    }

    bool created;
    ResourceId id;
    CompType typeHint;
    bool raw;
    ID3D11Resource *srvResource;
    ID3D11ShaderResourceView *srv[2];
  };

  static const int NUM_CACHED_SRVS = 64;

  std::list<CacheElem> m_ShaderItemCache;

  CacheElem &GetCachedElem(ResourceId id, CompType typeHint, bool raw);

  int m_width, m_height;
  float m_supersamplingX, m_supersamplingY;

  WrappedID3D11Device *m_WrappedDevice;
  WrappedID3D11DeviceContext *m_WrappedContext;

  D3D11ResourceManager *m_ResourceManager;

  ID3D11Device *m_pDevice;
  ID3D11DeviceContext *m_pImmediateContext;

  IDXGIFactory *m_pFactory;

  struct OutputWindow
  {
    HWND wnd;
    IDXGISwapChain *swap;
    ID3D11RenderTargetView *rtv;
    ID3D11DepthStencilView *dsv;

    WrappedID3D11Device *dev;

    void MakeRTV();
    void MakeDSV();

    int width, height;
  };

  uint64_t m_OutputWindowID;
  map<uint64_t, OutputWindow> m_OutputWindows;

  // used to track the real state so we can preserve it even
  // across work done to the output windows
  struct RealState
  {
    RealState() : state((Serialiser *)NULL) { active = false; }
    bool active;
    D3D11RenderState state;
  } m_RealState;

  static const uint32_t m_ShaderCacheMagic = 0xf000baba;
  static const uint32_t m_ShaderCacheVersion = 3;

  bool m_ShaderCacheDirty, m_CacheShaders;
  map<uint32_t, ID3DBlob *> m_ShaderCache;

  uint32_t m_SOBufferSize = 32 * 1024 * 1024;
  ID3D11Buffer *m_SOBuffer = NULL;
  ID3D11Buffer *m_SOStagingBuffer = NULL;
  std::vector<ID3D11Query *> m_SOStatsQueries;
  // event -> data
  map<uint32_t, D3D11PostVSData> m_PostVSData;

  HighlightCache m_HighlightCache;

  ID3D11Texture2D *m_OverlayRenderTex;
  ResourceId m_OverlayResourceId;

  ID3D11Texture2D *m_CustomShaderTex;
  ID3D11RenderTargetView *m_CustomShaderRTV;
  ResourceId m_CustomShaderResourceId;

  ID3D11BlendState *m_WireframeHelpersBS;
  ID3D11RasterizerState *m_WireframeHelpersRS, *m_WireframeHelpersCullCCWRS,
      *m_WireframeHelpersCullCWRS;
  ID3D11RasterizerState *m_SolidHelpersRS;

  // these gets updated to pull the elements selected out of the buffers
  ID3D11InputLayout *m_MeshDisplayLayout;

  // whenever these change
  ResourceFormat m_PrevMeshFmt;
  ResourceFormat m_PrevMeshFmt2;

  ID3D11Buffer *m_AxisHelper;
  ID3D11Buffer *m_FrustumHelper;
  ID3D11Buffer *m_TriHighlightHelper;

  bool InitStreamOut();
  void CreateSOBuffers();
  void ShutdownStreamOut();

  // font/text rendering
  bool InitFontRendering();
  void ShutdownFontRendering();

  void RenderTextInternal(float x, float y, const char *text);

  void CreateCustomShaderTex(uint32_t w, uint32_t h);

  void PixelHistoryCopyPixel(CopyPixelParams &params, uint32_t x, uint32_t y);

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  static const uint32_t STAGE_BUFFER_BYTE_SIZE = 4 * 1024 * 1024;

  struct FontData
  {
    FontData() { RDCEraseMem(this, sizeof(FontData)); }
    ~FontData()
    {
      SAFE_RELEASE(Tex);
      SAFE_RELEASE(CBuffer);
      SAFE_RELEASE(GlyphData);
      SAFE_RELEASE(CharBuffer);
      SAFE_RELEASE(VS);
      SAFE_RELEASE(PS);
    }

    ID3D11ShaderResourceView *Tex;
    ID3D11Buffer *CBuffer;
    ID3D11Buffer *GlyphData;
    ID3D11Buffer *CharBuffer;
    ID3D11VertexShader *VS;
    ID3D11PixelShader *PS;

    float CharAspect;
    float CharSize;
  } m_Font;

  struct DebugRenderData
  {
    DebugRenderData() { RDCEraseMem(this, sizeof(DebugRenderData)); }
    ~DebugRenderData()
    {
      SAFE_RELEASE(StageBuffer);

      SAFE_RELEASE(RastState);
      SAFE_RELEASE(BlendState);
      SAFE_RELEASE(NopBlendState);
      SAFE_RELEASE(PointSampState);
      SAFE_RELEASE(LinearSampState);
      SAFE_RELEASE(NoDepthState);
      SAFE_RELEASE(LEqualDepthState);
      SAFE_RELEASE(NopDepthState);
      SAFE_RELEASE(AllPassDepthState);
      SAFE_RELEASE(AllPassIncrDepthState);
      SAFE_RELEASE(StencIncrEqDepthState);

      SAFE_RELEASE(GenericLayout);
      SAFE_RELEASE(GenericVSCBuffer);
      SAFE_RELEASE(GenericGSCBuffer);
      SAFE_RELEASE(GenericPSCBuffer);
      SAFE_RELEASE(GenericVS);
      SAFE_RELEASE(TexDisplayPS);
      SAFE_RELEASE(CheckerboardPS);
      SAFE_RELEASE(OutlinePS);
      SAFE_RELEASE(MeshVS);
      SAFE_RELEASE(MeshGS);
      SAFE_RELEASE(MeshPS);
      SAFE_RELEASE(TriangleSizeGS);
      SAFE_RELEASE(TriangleSizePS);
      SAFE_RELEASE(FullscreenVS);
      SAFE_RELEASE(WireframePS);
      SAFE_RELEASE(OverlayPS);

      SAFE_RELEASE(CopyMSToArrayPS);
      SAFE_RELEASE(CopyArrayToMSPS);
      SAFE_RELEASE(FloatCopyMSToArrayPS);
      SAFE_RELEASE(FloatCopyArrayToMSPS);
      SAFE_RELEASE(DepthCopyMSToArrayPS);
      SAFE_RELEASE(DepthCopyArrayToMSPS);
      SAFE_RELEASE(PixelHistoryUnusedCS);
      SAFE_RELEASE(PixelHistoryCopyCS);
      SAFE_RELEASE(PrimitiveIDPS);

      SAFE_RELEASE(MeshPickCS);
      SAFE_RELEASE(PickIBBuf);
      SAFE_RELEASE(PickVBBuf);
      SAFE_RELEASE(PickIBSRV);
      SAFE_RELEASE(PickVBSRV);
      SAFE_RELEASE(PickResultBuf);
      SAFE_RELEASE(PickResultUAV);

      SAFE_RELEASE(QuadOverdrawPS);
      SAFE_RELEASE(QOResolvePS);

      SAFE_RELEASE(tileResultBuff);
      SAFE_RELEASE(resultBuff);
      SAFE_RELEASE(resultStageBuff);

      for(int i = 0; i < 3; i++)
      {
        SAFE_RELEASE(tileResultUAV[i]);
        SAFE_RELEASE(resultUAV[i]);
        SAFE_RELEASE(tileResultSRV[i]);
      }

      for(int i = 0; i < ARRAY_COUNT(TileMinMaxCS); i++)
      {
        for(int j = 0; j < 3; j++)
        {
          SAFE_RELEASE(TileMinMaxCS[i][j]);
          SAFE_RELEASE(HistogramCS[i][j]);

          if(i == 0)
            SAFE_RELEASE(ResultMinMaxCS[j]);
        }
      }

      SAFE_RELEASE(histogramBuff);
      SAFE_RELEASE(histogramStageBuff);

      SAFE_RELEASE(histogramUAV);

      SAFE_DELETE_ARRAY(MeshVSBytecode);

      SAFE_RELEASE(PickPixelRT);
      SAFE_RELEASE(PickPixelStageTex);

      for(int i = 0; i < ARRAY_COUNT(PublicCBuffers); i++)
      {
        SAFE_RELEASE(PublicCBuffers[i]);
      }
    }

    ID3D11Buffer *StageBuffer;

    ID3D11RasterizerState *RastState;
    ID3D11SamplerState *PointSampState, *LinearSampState;
    ID3D11BlendState *BlendState, *NopBlendState;
    ID3D11DepthStencilState *NoDepthState, *LEqualDepthState, *NopDepthState, *AllPassDepthState,
        *AllPassIncrDepthState, *StencIncrEqDepthState;

    ID3D11InputLayout *GenericLayout;
    ID3D11Buffer *GenericVSCBuffer;
    ID3D11Buffer *GenericGSCBuffer;
    ID3D11Buffer *GenericPSCBuffer;
    ID3D11Buffer *PublicCBuffers[20];
    ID3D11VertexShader *GenericVS, *MeshVS, *FullscreenVS;
    ID3D11GeometryShader *MeshGS, *TriangleSizeGS;
    ID3D11PixelShader *TexDisplayPS, *OverlayPS, *WireframePS, *MeshPS, *CheckerboardPS,
        *TriangleSizePS;
    ID3D11PixelShader *OutlinePS;
    ID3D11PixelShader *CopyMSToArrayPS, *CopyArrayToMSPS;
    ID3D11PixelShader *FloatCopyMSToArrayPS, *FloatCopyArrayToMSPS;
    ID3D11PixelShader *DepthCopyMSToArrayPS, *DepthCopyArrayToMSPS;
    ID3D11ComputeShader *PixelHistoryUnusedCS, *PixelHistoryCopyCS;
    ID3D11PixelShader *PrimitiveIDPS;

    static const uint32_t maxMeshPicks = 500;

    ID3D11ComputeShader *MeshPickCS;
    ID3D11Buffer *PickIBBuf, *PickVBBuf;
    uint32_t PickIBSize, PickVBSize;
    ID3D11ShaderResourceView *PickIBSRV, *PickVBSRV;
    ID3D11Buffer *PickResultBuf;
    ID3D11UnorderedAccessView *PickResultUAV;

    ID3D11PixelShader *QuadOverdrawPS, *QOResolvePS;

    ID3D11Buffer *tileResultBuff, *resultBuff, *resultStageBuff;
    ID3D11UnorderedAccessView *tileResultUAV[3], *resultUAV[3];
    ID3D11ShaderResourceView *tileResultSRV[3];
    ID3D11ComputeShader *TileMinMaxCS[eTexType_Max][3];    // uint, sint, float
    ID3D11ComputeShader *HistogramCS[eTexType_Max][3];     // uint, sint, float
    ID3D11ComputeShader *ResultMinMaxCS[3];
    ID3D11Buffer *histogramBuff, *histogramStageBuff;
    ID3D11UnorderedAccessView *histogramUAV;

    byte *MeshVSBytecode;
    uint32_t MeshVSBytelen;

    int publicCBufIdx;

    ID3D11RenderTargetView *PickPixelRT;
    ID3D11Texture2D *PickPixelStageTex;
  } m_DebugRender;

  bool InitDebugRendering();

  ShaderDebug::State CreateShaderDebugState(ShaderDebugTrace &trace, int quadIdx,
                                            DXBC::DXBCFile *dxbc, vector<byte> *cbufData);
  void CreateShaderGlobalState(ShaderDebug::GlobalState &global, DXBC::DXBCFile *dxbc,
                               uint32_t UAVStartSlot, ID3D11UnorderedAccessView **UAVs,
                               ID3D11ShaderResourceView **SRVs);
  void FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                            const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);
  friend struct ShaderDebugState;

  // called after the device is created, to init any counters
  void PostDeviceInitCounters();

  // called before the device is shutdown, to shutdown any counters
  void PreDeviceShutdownCounters();

  void FillTimers(D3D11CounterContext &ctx, const DrawcallTreeNode &drawnode);

  void FillTimersAMD(uint32_t &eventStartID, uint32_t &sampleIndex, vector<uint32_t> &eventIDs,
                     const DrawcallTreeNode &drawnode);

  void FillCBuffer(ID3D11Buffer *buf, const void *data, size_t size);
};
