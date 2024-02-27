/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <map>
#include "driver/dx/official/d3d11_4.h"
#include "replay/replay_driver.h"
#include "d3d11_common.h"

class Camera;
class Vec3f;

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;

class D3D11ResourceManager;

struct D3D11CopyPixelParams;

namespace DXBC
{
class DXBCContainer;
};

struct TextureShaderDetails
{
  TextureShaderDetails()
  {
    texFmt = srvFormat = DXGI_FORMAT_UNKNOWN;
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

  DXGI_FORMAT texFmt, srvFormat;
  UINT texWidth;
  UINT texHeight;
  UINT texDepth;
  UINT texMips;
  UINT texArraySize;

  UINT sampleCount;
  UINT sampleQuality;

  Vec4u YUVDownsampleRate;
  Vec4u YUVAChannels;

  D3D11TextureDetailsType texType;

  ID3D11Resource *srvResource;
  ID3D11Resource *previewCopy;

  ID3D11ShaderResourceView *srv[eTexType_Max];
};

class D3D11DebugManager
{
public:
  D3D11DebugManager(WrappedID3D11Device *wrapper);
  ~D3D11DebugManager();

  void RenderForPredicate();

  ResourceId AddCounterUAVBuffer(ID3D11UnorderedAccessView *uav);
  ResourceId GetCounterBufferID(ID3D11UnorderedAccessView *uav)
  {
    auto it = m_UAVToCounterBuffer.find(uav);
    if(it != m_UAVToCounterBuffer.end())
      return it->second;
    return ResourceId();
  }
  ID3D11UnorderedAccessView *GetCounterBufferUAV(ResourceId id)
  {
    auto it = m_CounterBufferToUAV.find(id);
    if(it != m_CounterBufferToUAV.end())
      return it->second;
    return NULL;
  }
  void GetCounterBuffers(rdcarray<BufferDescription> &ret)
  {
    for(auto pair : m_CounterBufferToUAV)
    {
      BufferDescription buf;

      // no original ID for this one
      buf.resourceId = pair.first;
      buf.length = 4;
      buf.gpuAddress = 0;
      buf.creationFlags = BufferCategory::ReadWrite;

      ret.push_back(buf);
    }
  }

  void FillWithDiscardPattern(DiscardType type, ID3D11Resource *res, UINT slice, UINT mip,
                              const D3D11_RECT *pRect, UINT NumRects);
  void FillWithDiscardPattern(DiscardType type, ID3D11Resource *res, UINT subresource,
                              const D3D11_RECT *pRect, UINT NumRects);
  void FillWithDiscardPattern(DiscardType type, ID3D11View *view, const D3D11_RECT *pRect,
                              UINT NumRects);

  uint32_t GetStructCount(ID3D11UnorderedAccessView *uav);
  void GetBufferData(ID3D11Buffer *buff, uint64_t offset, uint64_t length, bytebuf &retData);

  void CopyArrayToTex2DMS(ID3D11Texture2D *destMS, ID3D11Texture2D *srcArray, UINT selectedSlice);
  void CopyTex2DMSToArray(ID3D11Texture2D *destArray, ID3D11Texture2D *srcMS);

  ID3D11Buffer *MakeCBuffer(const void *data, size_t size);
  ID3D11Buffer *MakeCBuffer(UINT size);

  void FillCBuffer(ID3D11Buffer *buf, const void *data, size_t size);

  TextureShaderDetails GetShaderDetails(ResourceId id, CompType typeCast, bool rawOutput);

  void PixelHistoryCopyPixel(D3D11CopyPixelParams &params, size_t eventSlot, uint32_t storeSlot);

  struct CacheElem
  {
    CacheElem(ResourceId id_, CompType typeHint_, bool raw_)
        : created(false), id(id_), typeCast(typeHint_), raw(raw_), srvResource(NULL)
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
    CompType typeCast;
    bool raw;
    ID3D11Resource *srvResource;
    ID3D11ShaderResourceView *srv[2];
  };

  CacheElem &GetCachedElem(ResourceId id, CompType typeCast, bool raw);

private:
  void InitCommonResources();
  void InitReplayResources();
  void ShutdownResources();

  static const int NUM_CACHED_SRVS = 64;
  static const uint32_t STAGE_BUFFER_BYTE_SIZE = 4 * 1024 * 1024;

  rdcarray<CacheElem> m_ShaderItemCache;

  std::map<ResourceId, ID3D11UnorderedAccessView *> m_CounterBufferToUAV;
  std::map<ID3D11UnorderedAccessView *, ResourceId> m_UAVToCounterBuffer;

  WrappedID3D11Device *m_pDevice = NULL;
  WrappedID3D11DeviceContext *m_pImmediateContext = NULL;

  // MakeCBuffer
  int publicCBufIdx = 0;
  ID3D11Buffer *PublicCBuffers[20] = {NULL};
  static const size_t PublicCBufferSize = sizeof(float) * 1024;

  // GetBufferData
  ID3D11Buffer *StageBuffer = NULL;

  // CopyArrayToTex2DMS & CopyTex2DMSToArray
  ID3D11VertexShader *MSArrayCopyVS = NULL;
  ID3D11PixelShader *CopyMSToArrayPS = NULL;
  ID3D11PixelShader *CopyArrayToMSPS = NULL;
  ID3D11PixelShader *FloatCopyMSToArrayPS = NULL;
  ID3D11PixelShader *FloatCopyArrayToMSPS = NULL;
  ID3D11PixelShader *DepthCopyMSToArrayPS = NULL;
  ID3D11PixelShader *DepthCopyArrayToMSPS = NULL;

  // PixelHistoryCopyPixel
  ID3D11ComputeShader *PixelHistoryUnusedCS = NULL;
  ID3D11ComputeShader *PixelHistoryCopyCS = NULL;

  // RenderForPredicate
  ID3D11DepthStencilView *PredicateDSV = NULL;

  struct DiscardPatternKey
  {
    uint32_t dim;
    DXGI_FORMAT fmt;
    DXGI_SAMPLE_DESC samp;

    bool operator<(const DiscardPatternKey &o) const
    {
      if(dim != o.dim)
        return dim < o.dim;
      if(fmt != o.fmt)
        return fmt < o.fmt;
      return samp.Count < o.samp.Count;
    }
  };
  std::map<DiscardPatternKey, ID3D11Resource *> m_DiscardPatterns;
  bytebuf m_DiscardBytes;
  ID3D11VertexShader *m_DiscardVS = NULL;
  ID3D11PixelShader *m_DiscardFloatPS = NULL;
  ID3D11PixelShader *m_DiscardIntPS = NULL;
  ID3D11DepthStencilState *m_DiscardDepthState = NULL;
  ID3D11RasterizerState *m_DiscardRasterState = NULL;
};
