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

#include "d3d11_debug.h"
#include "common/shader_cache.h"
#include "data/resource.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "d3d11_context.h"
#include "d3d11_manager.h"
#include "d3d11_renderstate.h"
#include "d3d11_replay.h"
#include "d3d11_resources.h"
#include "d3d11_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

static void InternalRef(ID3D11DeviceChild *child)
{
  if(child)
  {
    // we don't want any of our internal resources to show up as external references but we do
    // certainly want to keep them alive.
    // To solve this, we add an internal refcount and remove the implicit external refcount.
    // Removing the external refcount means the device will also have no refcount so it will still
    // be destroyed at the same time regardless of these objects. The internal ref will keep them
    // alive even if the user doesn't have any pointers to them and they aren't bound anywhere.
    // Effectively you can think of this as the same as if they were bound to some unknown undefined
    // pipeline slot
    IntAddRef(child);
    child->Release();
  }
}

D3D11DebugManager::D3D11DebugManager(WrappedID3D11Device *wrapper)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(D3D11DebugManager));

  m_pDevice = wrapper;
  m_pImmediateContext = wrapper->GetImmediateContext();

  m_pDevice->GetShaderCache()->SetCaching(true);

  // create things needed both during capture and replay
  InitCommonResources();

  // now do replay-only initialisation
  if(RenderDoc::Inst().IsReplayApp())
    InitReplayResources();

  m_pDevice->GetShaderCache()->SetCaching(false);
}

D3D11DebugManager::~D3D11DebugManager()
{
  ShutdownResources();

  while(!m_ShaderItemCache.empty())
  {
    CacheElem &elem = m_ShaderItemCache.back();
    elem.Release();
    m_ShaderItemCache.pop_back();
  }

  RenderDoc::Inst().UnregisterMemoryRegion(this);
}

//////////////////////////////////////////////////////
// debug/replay functions

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(UINT size)
{
  D3D11_BUFFER_DESC bufDesc;

  bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bufDesc.Usage = D3D11_USAGE_DYNAMIC;
  bufDesc.ByteWidth = size;
  bufDesc.StructureByteStride = 0;
  bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bufDesc.MiscFlags = 0;

  ID3D11Buffer *ret = NULL;

  HRESULT hr = m_pDevice->CreateBuffer(&bufDesc, NULL, &ret);

  if(FAILED(hr))
  {
    RDCERR("Failed to create CBuffer HRESULT: %s", ToStr(hr).c_str());
    return NULL;
  }

  return ret;
}

void D3D11DebugManager::FillCBuffer(ID3D11Buffer *buf, const void *data, size_t size)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->GetReal()->Map(UNWRAP(WrappedID3D11Buffer, buf), 0,
                                                   D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  m_pDevice->CheckHRESULT(hr);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    memcpy(mapped.pData, data, size);
    m_pImmediateContext->GetReal()->Unmap(UNWRAP(WrappedID3D11Buffer, buf), 0);
  }
}

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(const void *data, size_t size)
{
  int idx = publicCBufIdx;

  RDCASSERT(size <= PublicCBufferSize, size, PublicCBufferSize);

  FillCBuffer(PublicCBuffers[idx], data, RDCMIN(size, PublicCBufferSize));

  publicCBufIdx = (publicCBufIdx + 1) % ARRAY_COUNT(PublicCBuffers);

  return PublicCBuffers[idx];
}

void D3D11DebugManager::InitCommonResources()
{
  D3D11ShaderCache *shaderCache = m_pDevice->GetShaderCache();
  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  rdcstr multisamplehlsl = GetEmbeddedResource(multisample_hlsl);
  rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

  if(m_pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
  {
    CopyMSToArrayPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyMSToArray", "ps_5_0");
    InternalRef(CopyMSToArrayPS);
    CopyArrayToMSPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyArrayToMS", "ps_5_0");
    InternalRef(CopyArrayToMSPS);
    FloatCopyMSToArrayPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyMSToArray", "ps_5_0");
    InternalRef(FloatCopyMSToArrayPS);
    FloatCopyArrayToMSPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyArrayToMS", "ps_5_0");
    InternalRef(FloatCopyArrayToMSPS);
    DepthCopyMSToArrayPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyMSToArray", "ps_5_0");
    InternalRef(DepthCopyMSToArrayPS);
    DepthCopyArrayToMSPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyArrayToMS", "ps_5_0");
    InternalRef(DepthCopyArrayToMSPS);
    MSArrayCopyVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");
    InternalRef(MSArrayCopyVS);
  }
  else
  {
    RDCWARN("Device feature level below 11_0, MSAA <-> Array copies will not be supported.");
    CopyMSToArrayPS = NULL;
    CopyArrayToMSPS = NULL;
    FloatCopyMSToArrayPS = NULL;
    FloatCopyArrayToMSPS = NULL;
    DepthCopyMSToArrayPS = NULL;
    DepthCopyArrayToMSPS = NULL;
  }

  // mark created resources as internal during capture so they aren't included in capture files.
  rm->SetInternalResource(CopyMSToArrayPS);
  rm->SetInternalResource(CopyArrayToMSPS);
  rm->SetInternalResource(FloatCopyMSToArrayPS);
  rm->SetInternalResource(FloatCopyArrayToMSPS);
  rm->SetInternalResource(DepthCopyMSToArrayPS);
  rm->SetInternalResource(DepthCopyArrayToMSPS);
  rm->SetInternalResource(MSArrayCopyVS);

  for(int i = 0; i < ARRAY_COUNT(PublicCBuffers); i++)
  {
    PublicCBuffers[i] = MakeCBuffer(PublicCBufferSize);
    InternalRef(PublicCBuffers[i]);
    rm->SetInternalResource(PublicCBuffers[i]);
  }

  publicCBufIdx = 0;
}

void D3D11DebugManager::InitReplayResources()
{
  D3D11ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  HRESULT hr = S_OK;

  {
    rdcstr hlsl = GetEmbeddedResource(pixelhistory_hlsl);

    PixelHistoryUnusedCS =
        shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_PixelHistoryUnused", "cs_5_0");
    PixelHistoryCopyCS =
        shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_PixelHistoryCopyPixel", "cs_5_0");
  }

  RDCCOMPILE_ASSERT(eTexType_1D == RESTYPE_TEX1D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_2D == RESTYPE_TEX2D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_3D == RESTYPE_TEX3D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_Depth == RESTYPE_DEPTH, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_Stencil == RESTYPE_DEPTH_STENCIL,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_DepthMS == RESTYPE_DEPTH_MS,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_StencilMS == RESTYPE_DEPTH_STENCIL_MS,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_2DMS == RESTYPE_TEX2D_MS,
                    "Tex type enum doesn't match shader defines");

  {
    D3D11_BUFFER_DESC desc;

    desc.StructureByteStride = 0;
    desc.ByteWidth = STAGE_BUFFER_BYTE_SIZE;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&desc, NULL, &StageBuffer);

    if(FAILED(hr))
      RDCERR("Failed to create map staging buffer HRESULT: %s", ToStr(hr).c_str());
  }

  {
    D3D11_TEXTURE2D_DESC desc = {};

    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D16_UNORM;
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D *dummyTex = NULL;
    hr = m_pDevice->CreateTexture2D(&desc, NULL, &dummyTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create dummy predicate tex HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      hr = m_pDevice->CreateDepthStencilView(dummyTex, NULL, &PredicateDSV);

      if(FAILED(hr))
      {
        RDCERR("Failed to predicate DSV HRESULT: %s", ToStr(hr).c_str());
      }
    }

    SAFE_RELEASE(dummyTex);
  }

  {
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

    m_DiscardVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");
    m_DiscardFloatPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DiscardFloatPS", "ps_4_0");
    m_DiscardIntPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DiscardIntPS", "ps_4_0");

    ResourceFormat fmt;
    fmt.type = ResourceFormatType::Regular;
    fmt.compType = CompType::Float;
    fmt.compByteWidth = 4;
    fmt.compCount = 1;
    m_DiscardBytes = GetDiscardPattern(DiscardType::DiscardCall, fmt);
    fmt.compType = CompType::SInt;
    m_DiscardBytes.append(GetDiscardPattern(DiscardType::DiscardCall, fmt));

    D3D11_DEPTH_STENCIL_DESC desc;

    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_REPLACE;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.StencilReadMask = desc.StencilWriteMask = 0xff;
    desc.StencilEnable = TRUE;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DiscardDepthState);

    if(FAILED(hr))
      RDCERR("Failed to create m_DiscardDepthState HRESULT: %s", ToStr(hr).c_str());

    D3D11_RASTERIZER_DESC rastDesc;
    RDCEraseEl(rastDesc);

    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.DepthClipEnable = FALSE;
    rastDesc.ScissorEnable = TRUE;

    hr = m_pDevice->CreateRasterizerState(&rastDesc, &m_DiscardRasterState);

    if(FAILED(hr))
      RDCERR("Failed to create m_DiscardRasterState HRESULT: %s", ToStr(hr).c_str());
  }
}

void D3D11DebugManager::ShutdownResources()
{
  SAFE_RELEASE(StageBuffer);

  SAFE_RELEASE(PredicateDSV);

  // the objects we added with an internal ref, because they're used during capture, should also be
  // released with an internal ref.
  SAFE_INTRELEASE(CopyMSToArrayPS);
  SAFE_INTRELEASE(CopyArrayToMSPS);
  SAFE_INTRELEASE(FloatCopyMSToArrayPS);
  SAFE_INTRELEASE(FloatCopyArrayToMSPS);
  SAFE_INTRELEASE(DepthCopyMSToArrayPS);
  SAFE_INTRELEASE(DepthCopyArrayToMSPS);
  SAFE_INTRELEASE(MSArrayCopyVS);

  SAFE_RELEASE(PixelHistoryUnusedCS);
  SAFE_RELEASE(PixelHistoryCopyCS);

  for(auto it = m_DiscardPatterns.begin(); it != m_DiscardPatterns.end(); it++)
    if(it->second)
      it->second->Release();

  SAFE_RELEASE(m_DiscardVS);
  SAFE_RELEASE(m_DiscardFloatPS);
  SAFE_RELEASE(m_DiscardIntPS);
  SAFE_RELEASE(m_DiscardDepthState);
  SAFE_RELEASE(m_DiscardRasterState);

  for(int i = 0; i < ARRAY_COUNT(PublicCBuffers); i++)
  {
    SAFE_INTRELEASE(PublicCBuffers[i]);
  }
}

void D3D11DebugManager::FillWithDiscardPattern(DiscardType type, ID3D11Resource *res, UINT slice,
                                               UINT mip, const D3D11_RECT *pRect, UINT NumRects)
{
  D3D11MarkerRegion region(StringFormat::Fmt("FillWithDiscardPattern %s slice %u mip %u",
                                             ToStr(GetIDForDeviceChild(res)).c_str(), slice, mip));

  D3D11_RECT all = {0, 0, 65536, 65536};
  if(NumRects == 0)
  {
    NumRects = 1;
    pRect = &all;
  }

  if(WrappedID3D11Buffer::IsAlloc(res))
  {
    D3D11MarkerRegion::Set("Buffer");

    WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)res;

    D3D11_BUFFER_DESC desc = {};
    buf->GetDesc(&desc);

    uint32_t value = 0xD15CAD3D;

    for(UINT r = 0; r < NumRects; r++)
    {
      UINT size = RDCMIN(UINT(pRect[r].right - pRect[r].left), desc.ByteWidth);

      if(desc.Usage == D3D11_USAGE_DYNAMIC || desc.Usage == D3D11_USAGE_STAGING)
      {
        // dynamic buffers can always be mapped
        // staging buffers can be read-only, at which point we can't write to them
        if(desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
        {
          D3D11_MAPPED_SUBRESOURCE mapped = {};
          D3D11_MAP mapping =
              (desc.Usage == D3D11_USAGE_DYNAMIC) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
          HRESULT hr = m_pImmediateContext->Map(res, 0, mapping, 0, &mapped);
          m_pDevice->CheckHRESULT(hr);

          if(SUCCEEDED(hr))
          {
            byte *dst = (byte *)mapped.pData;
            dst += pRect[r].left;
            size_t copyStride = sizeof(uint32_t);
            for(size_t i = 0; i < size; i += copyStride)
            {
              memcpy(dst, &value, RDCMIN(sizeof(uint32_t), size - i));
              dst += copyStride;
            }

            m_pImmediateContext->Unmap(res, 0);
          }
          else
          {
            RDCERR("Couldn't fill discard pattern: %s", ToStr(hr).c_str());
            return;
          }
        }
      }
      else if(desc.Usage == D3D11_USAGE_DEFAULT)
      {
        bytebuf pattern;
        pattern.resize(AlignUp4(size));

        for(size_t i = 0; i < pattern.size(); i += 4)
          memcpy(&pattern[i], &value, sizeof(uint32_t));

        // default buffers can be updated
        D3D11_BOX box = {};
        box.bottom = box.back = 1;
        box.left = pRect[r].left;
        box.right = box.left + size;
        m_pImmediateContext->UpdateSubresource(res, 0, &box, pattern.data(), size, size);
      }
      // IMMUTABLE is the other option, which we can't do anything with
    }
  }
  else
  {
    DiscardPatternKey key = {};
    UINT numMips = 1;
    UINT width = 1, height = 1, depth = 1;

    // for textures we create a template texture with the same format and properties then do
    // repeated copies
    if(WrappedID3D11Texture1D::IsAlloc(res))
    {
      WrappedID3D11Texture1D *tex = (WrappedID3D11Texture1D *)res;

      D3D11_TEXTURE1D_DESC desc = {};
      tex->GetDesc(&desc);

      key.dim = 1;
      key.fmt = desc.Format;
      numMips = desc.MipLevels;
      width = desc.Width;
    }
    else if(WrappedID3D11Texture2D1::IsAlloc(res))
    {
      WrappedID3D11Texture2D1 *tex = (WrappedID3D11Texture2D1 *)res;

      D3D11_TEXTURE2D_DESC desc = {};
      tex->GetDesc(&desc);

      key.dim = 2;
      key.fmt = desc.Format;
      key.samp = desc.SampleDesc;
      numMips = desc.MipLevels;
      width = desc.Width;
      height = desc.Height;
    }
    else if(WrappedID3D11Texture3D1::IsAlloc(res))
    {
      WrappedID3D11Texture3D1 *tex = (WrappedID3D11Texture3D1 *)res;

      D3D11_TEXTURE3D_DESC desc = {};
      tex->GetDesc(&desc);

      key.dim = 3;
      key.fmt = desc.Format;
      numMips = desc.MipLevels;
      width = desc.Width;
      height = desc.Height;
      depth = desc.Depth;
    }

    UINT subresource = slice * numMips + mip;
    if(key.dim == 3)
      subresource = mip;

    // depth-stencil resources can't be sub-copied, so we need to render to them
    if(IsDepthFormat(key.fmt) || key.samp.Count > 1)
    {
      D3D11MarkerRegion::Set("Depth/MSAA texture");

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.Format = GetTypedFormat(key.fmt, CompType::Float);

      D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
      dsvDesc.Flags = 0;
      dsvDesc.Format = GetDepthTypedFormat(key.fmt);

      bool intFormat = !IsDepthFormat(key.fmt) && (IsIntFormat(key.fmt) || IsUIntFormat(key.fmt));

      D3D11RenderStateTracker tracker(m_pImmediateContext);

      m_pImmediateContext->ClearState();

      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      m_pImmediateContext->OMSetDepthStencilState(m_DiscardDepthState, 0);
      m_pImmediateContext->VSSetShader(m_DiscardVS, NULL, 0);
      m_pImmediateContext->PSSetShader(intFormat ? m_DiscardIntPS : m_DiscardFloatPS, NULL, 0);
      m_pImmediateContext->RSSetState(m_DiscardRasterState);

      if(key.samp.Count > 1)
      {
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
        dsvDesc.Texture2DMSArray.ArraySize = 1;
        dsvDesc.Texture2DMSArray.FirstArraySlice = slice;

        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
        rtvDesc.Texture2DMSArray.ArraySize = 1;
        rtvDesc.Texture2DMSArray.FirstArraySlice = slice;
      }
      else if(key.dim == 1)
      {
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
        dsvDesc.Texture1DArray.ArraySize = 1;
        dsvDesc.Texture1DArray.FirstArraySlice = slice;
        dsvDesc.Texture1DArray.MipSlice = mip;
      }
      else if(key.dim == 2)
      {
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.ArraySize = 1;
        dsvDesc.Texture2DArray.FirstArraySlice = slice;
        dsvDesc.Texture2DArray.MipSlice = mip;
      }

      ID3D11RenderTargetView *rtv = NULL;
      ID3D11DepthStencilView *dsv = NULL;
      if(IsDepthFormat(key.fmt))
        m_pDevice->CreateDepthStencilView(res, &dsvDesc, &dsv);
      else
        m_pDevice->CreateRenderTargetView(res, &rtvDesc, &rtv);

      m_pImmediateContext->OMSetRenderTargets(1, &rtv, dsv);

      ID3D11Buffer *cbuf = MakeCBuffer(m_DiscardBytes.data(), m_DiscardBytes.size());
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

      D3D11_VIEWPORT viewport = {0, 0, (float)width, (float)height, 0.0f, 1.0f};
      m_pImmediateContext->RSSetViewports(1, &viewport);

      for(UINT r = 0; r < NumRects; r++)
      {
        m_pImmediateContext->RSSetScissorRects(1, pRect + r);

        if(dsv)
        {
          uint32_t pass = 1;
          cbuf = MakeCBuffer(&pass, sizeof(pass));
          m_pImmediateContext->PSSetConstantBuffers(1, 1, &cbuf);

          m_pImmediateContext->Draw(3, 0);

          m_pImmediateContext->OMSetDepthStencilState(m_DiscardDepthState, 0xff);
          pass = 2;
          cbuf = MakeCBuffer(&pass, sizeof(pass));
          m_pImmediateContext->PSSetConstantBuffers(1, 1, &cbuf);

          m_pImmediateContext->Draw(3, 0);
        }
        else
        {
          uint32_t pass = 0;
          cbuf = MakeCBuffer(&pass, sizeof(pass));
          m_pImmediateContext->PSSetConstantBuffers(1, 1, &cbuf);

          m_pImmediateContext->Draw(3, 0);
        }
      }

      SAFE_RELEASE(rtv);
      SAFE_RELEASE(dsv);
    }
    else
    {
      ID3D11Resource *patternRes = m_DiscardPatterns[key];

      if(patternRes == NULL)
      {
        bytebuf pattern = GetDiscardPattern(type, MakeResourceFormat(key.fmt));

        if(key.dim == 1)
        {
          D3D11_TEXTURE1D_DESC desc;

          desc.ArraySize = 1;
          desc.Format = key.fmt;
          desc.Width = DiscardPatternWidth;
          desc.MipLevels = 1;
          desc.Usage = D3D11_USAGE_IMMUTABLE;
          desc.CPUAccessFlags = 0;
          desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
          desc.MiscFlags = 0;

          D3D11_SUBRESOURCE_DATA data = {};
          data.pSysMem = pattern.data();
          data.SysMemSlicePitch = data.SysMemPitch = GetRowPitch(desc.Width, desc.Format, 0);

          ID3D11Texture1D *tex = NULL;

          HRESULT hr = m_pDevice->CreateTexture1D(&desc, &data, &tex);
          if(FAILED(hr))
          {
            RDCERR("Failed to create discard texture for %s HRESULT: %s", ToStr(key.fmt).c_str(),
                   ToStr(hr).c_str());
            return;
          }

          m_DiscardPatterns[key] = patternRes = tex;
        }
        else if(key.dim == 2)
        {
          D3D11_TEXTURE2D_DESC desc;

          desc.ArraySize = 1;
          desc.Format = key.fmt;
          desc.Width = DiscardPatternWidth;
          desc.Height = DiscardPatternHeight;
          desc.MipLevels = 1;
          desc.SampleDesc.Count = 1;
          desc.SampleDesc.Quality = 0;
          desc.Usage = D3D11_USAGE_IMMUTABLE;
          desc.CPUAccessFlags = 0;
          desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
          desc.MiscFlags = 0;

          D3D11_SUBRESOURCE_DATA data = {};
          data.pSysMem = pattern.data();
          data.SysMemPitch = GetRowPitch(desc.Width, desc.Format, 0);
          data.SysMemSlicePitch = data.SysMemPitch * desc.Height;

          ID3D11Texture2D *tex = NULL;

          HRESULT hr = m_pDevice->CreateTexture2D(&desc, &data, &tex);
          if(FAILED(hr))
          {
            RDCERR("Failed to create discard texture for %s HRESULT: %s", ToStr(key.fmt).c_str(),
                   ToStr(hr).c_str());
            return;
          }

          m_DiscardPatterns[key] = patternRes = tex;
        }
        else
        {
          D3D11_TEXTURE3D_DESC desc;

          desc.Format = key.fmt;
          desc.Width = DiscardPatternWidth;
          desc.Height = DiscardPatternHeight;
          desc.Depth = 1;
          desc.MipLevels = 1;
          desc.Usage = D3D11_USAGE_IMMUTABLE;
          desc.CPUAccessFlags = 0;
          desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
          desc.MiscFlags = 0;

          D3D11_SUBRESOURCE_DATA data = {};
          data.pSysMem = pattern.data();
          data.SysMemPitch = GetRowPitch(desc.Width, desc.Format, 0);
          data.SysMemSlicePitch = data.SysMemPitch * desc.Height;

          ID3D11Texture3D *tex = NULL;

          HRESULT hr = m_pDevice->CreateTexture3D(&desc, &data, &tex);
          if(FAILED(hr))
          {
            RDCERR("Failed to create discard texture for %s HRESULT: %s", ToStr(key.fmt).c_str(),
                   ToStr(hr).c_str());
            return;
          }

          m_DiscardPatterns[key] = patternRes = tex;
        }
      }

      if(!patternRes)
        return;

      UINT z = 0;
      if(key.dim == 3)
        z = slice;

      for(UINT r = 0; r < NumRects; r++)
      {
        D3D11_RECT rect = pRect[r];

        UINT rectWidth = RDCMIN((UINT)rect.right, RDCMAX(1U, width >> mip));
        UINT rectHeight = RDCMIN((UINT)rect.bottom, RDCMAX(1U, height >> mip));

        for(UINT y = rect.top; y < rectHeight; y += DiscardPatternHeight)
        {
          for(UINT x = rect.left; x < rectWidth; x += DiscardPatternWidth)
          {
            D3D11_BOX box = {
                0,
                0,
                0,
                RDCMIN(DiscardPatternWidth, uint32_t(rectWidth - x)),
                RDCMIN(DiscardPatternHeight, uint32_t(rectHeight - y)),
                1,
            };
            m_pImmediateContext->CopySubresourceRegion(res, subresource, x, y, z, patternRes, 0,
                                                       &box);
          }
        }
      }
    }
  }
}

void D3D11DebugManager::FillWithDiscardPattern(DiscardType type, ID3D11Resource *res,
                                               UINT subresource, const D3D11_RECT *pRect,
                                               UINT NumRects)
{
  if(WrappedID3D11Texture1D::IsAlloc(res))
  {
    WrappedID3D11Texture1D *tex = (WrappedID3D11Texture1D *)res;

    D3D11_TEXTURE1D_DESC desc = {};
    tex->GetDesc(&desc);

    // subresource uniquely identifies a slice and mip
    FillWithDiscardPattern(type, res, subresource / desc.MipLevels, subresource % desc.MipLevels,
                           pRect, NumRects);
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(res))
  {
    WrappedID3D11Texture2D1 *tex = (WrappedID3D11Texture2D1 *)res;

    D3D11_TEXTURE2D_DESC desc = {};
    tex->GetDesc(&desc);

    // subresource uniquely identifies a slice and mip
    FillWithDiscardPattern(type, res, subresource / desc.MipLevels, subresource % desc.MipLevels,
                           pRect, NumRects);
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(res))
  {
    WrappedID3D11Texture3D1 *tex = (WrappedID3D11Texture3D1 *)res;

    D3D11_TEXTURE3D_DESC desc = {};
    tex->GetDesc(&desc);

    // fill all slices in this mip
    for(UINT z = 0; z < RDCMAX(1U, desc.Depth >> subresource); z++)
      FillWithDiscardPattern(type, res, z, subresource, pRect, NumRects);
  }
  else if(WrappedID3D11Buffer::IsAlloc(res))
  {
    // buffer
    FillWithDiscardPattern(type, res, 0, 0, pRect, NumRects);
  }
  else
  {
    RDCERR("Unknown resource type being discarded");
    return;
  }
}

void D3D11DebugManager::FillWithDiscardPattern(DiscardType type, ID3D11View *view,
                                               const D3D11_RECT *pRect, UINT NumRects)
{
  D3D11MarkerRegion region(StringFormat::Fmt("FillWithDiscardPattern view %s",
                                             ToStr(GetIDForDeviceChild(view)).c_str()));

  ResourceRange range = ResourceRange::Null;

  if(WrappedID3D11ShaderResourceView1::IsAlloc(view))
  {
    range = ResourceRange((WrappedID3D11ShaderResourceView1 *)view);
  }
  else if(WrappedID3D11UnorderedAccessView1::IsAlloc(view))
  {
    range = ResourceRange((WrappedID3D11UnorderedAccessView1 *)view);
  }
  else if(WrappedID3D11RenderTargetView1::IsAlloc(view))
  {
    range = ResourceRange((WrappedID3D11RenderTargetView1 *)view);
  }
  else if(WrappedID3D11DepthStencilView::IsAlloc(view))
  {
    range = ResourceRange((WrappedID3D11DepthStencilView *)view);
  }
  else
  {
    RDCERR("Unknown view type being discarded");
    return;
  }

  ID3D11Resource *res = range.GetResource();
  UINT numMips = 1;
  bool tex3D = false;

  // check for wrapped types first as they will be most common and don't
  // require a virtual call
  if(WrappedID3D11Texture1D::IsAlloc(res))
  {
    D3D11_TEXTURE1D_DESC desc = {};
    ((WrappedID3D11Texture1D *)res)->GetDesc(&desc);
    numMips = desc.MipLevels;
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(res))
  {
    D3D11_TEXTURE2D_DESC desc = {};
    ((WrappedID3D11Texture2D1 *)res)->GetDesc(&desc);
    numMips = desc.MipLevels;
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(res))
  {
    D3D11_TEXTURE3D_DESC desc = {};
    ((WrappedID3D11Texture3D1 *)res)->GetDesc(&desc);
    numMips = desc.MipLevels;
    tex3D = true;
  }
  else if(WrappedID3D11Buffer::IsAlloc(res))
  {
    // nothing to do, just pass
  }
  else
  {
    RDCERR("View of unknown resource type being discarded");
    return;
  }

  rdcarray<D3D11_RECT> rects;
  rects.assign(pRect, NumRects);

  // DiscardView1 on D3D11 only allows rects to be specified with DSVs and RTVs which should only
  // target one mip.
  if(NumRects > 0)
    RDCASSERTMSG("Rects shouldn't be specified when we have multiple mips",
                 range.GetMinMip() == range.GetMaxMip(), range.GetMinMip(), range.GetMaxMip(),
                 NumRects);

  for(UINT slice = range.GetMinSlice(); slice <= range.GetMaxSlice(); slice++)
    for(UINT mip = range.GetMinMip(); mip <= range.GetMaxMip(); mip++)
      FillWithDiscardPattern(type, res, slice, mip, pRect, NumRects);
}

uint32_t D3D11DebugManager::GetStructCount(ID3D11UnorderedAccessView *uav)
{
  m_pImmediateContext->CopyStructureCount(StageBuffer, 0, uav);

  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = m_pImmediateContext->Map(StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);
  m_pDevice->CheckHRESULT(hr);

  if(FAILED(hr))
  {
    RDCERR("Failed to Map HRESULT: %s", ToStr(hr).c_str());
    return ~0U;
  }

  uint32_t ret = *((uint32_t *)mapped.pData);

  m_pImmediateContext->Unmap(StageBuffer, 0);

  return ret;
}

void D3D11DebugManager::GetBufferData(ID3D11Buffer *buffer, uint64_t offset, uint64_t length,
                                      bytebuf &ret)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  if(buffer == NULL)
    return;

  RDCASSERT(offset < 0xffffffff);
  RDCASSERT(length <= 0xffffffff || length == ~0ULL);

  uint32_t offs = (uint32_t)offset;
  uint32_t len = (uint32_t)length;

  D3D11_BUFFER_DESC desc;
  buffer->GetDesc(&desc);

  if(offs >= desc.ByteWidth)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(len == 0 || len > desc.ByteWidth)
  {
    len = desc.ByteWidth - offs;
  }

  if(len > 0 && offs + len > desc.ByteWidth)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%u)",
            offset, length, desc.ByteWidth);
    len = RDCMIN(len, desc.ByteWidth - offs);
  }

  uint32_t outOffs = 0;

  ret.resize(len);

  D3D11_BOX box;
  box.top = 0;
  box.bottom = 1;
  box.front = 0;
  box.back = 1;

  while(len > 0)
  {
    uint32_t chunkSize = RDCMIN(len, STAGE_BUFFER_BYTE_SIZE);

    if(desc.StructureByteStride > 0)
      chunkSize -= (chunkSize % desc.StructureByteStride);

    box.left = RDCMIN(offs + outOffs, desc.ByteWidth);
    box.right = RDCMIN(offs + outOffs + chunkSize, desc.ByteWidth);

    if(box.right - box.left == 0)
      break;

    m_pImmediateContext->GetReal()->CopySubresourceRegion(
        UNWRAP(WrappedID3D11Buffer, StageBuffer), 0, 0, 0, 0, UNWRAP(WrappedID3D11Buffer, buffer),
        0, &box);

    HRESULT hr = m_pImmediateContext->GetReal()->Map(UNWRAP(WrappedID3D11Buffer, StageBuffer), 0,
                                                     D3D11_MAP_READ, 0, &mapped);
    m_pDevice->CheckHRESULT(hr);

    if(FAILED(hr))
    {
      RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
      return;
    }
    else
    {
      memcpy(&ret[outOffs], mapped.pData, RDCMIN(len, STAGE_BUFFER_BYTE_SIZE));

      m_pImmediateContext->GetReal()->Unmap(UNWRAP(WrappedID3D11Buffer, StageBuffer), 0);
    }

    outOffs += chunkSize;
    len -= chunkSize;
  }
}

void D3D11DebugManager::RenderForPredicate()
{
  // just somehow draw a quad that renders some pixels to fill the predicate with TRUE
  m_pImmediateContext->ClearState();
  D3D11_VIEWPORT viewport = {0, 0, 1, 1, 0.0f, 1.0f};
  m_pImmediateContext->RSSetViewports(1, &viewport);
  m_pImmediateContext->VSSetShader(MSArrayCopyVS, NULL, 0);
  m_pImmediateContext->PSSetShader(NULL, NULL, 0);
  m_pImmediateContext->OMSetRenderTargets(0, NULL, PredicateDSV);
  m_pImmediateContext->Draw(3, 0);
}

ResourceId D3D11DebugManager::AddCounterUAVBuffer(ID3D11UnorderedAccessView *uav)
{
  ResourceId ret = ResourceIDGen::GetNewUniqueID();
  m_CounterBufferToUAV[ret] = uav;
  m_UAVToCounterBuffer[uav] = ret;
  return ret;
}

void D3D11Replay::GeneralMisc::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  D3D11_RASTERIZER_DESC rastDesc;
  RDCEraseEl(rastDesc);

  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.FillMode = D3D11_FILL_SOLID;
  rastDesc.DepthBias = 0;

  hr = device->CreateRasterizerState(&rastDesc, &RasterState);

  if(FAILED(hr))
    RDCERR("Failed to create default rasterizer state HRESULT: %s", ToStr(hr).c_str());

  rastDesc.ScissorEnable = TRUE;

  hr = device->CreateRasterizerState(&rastDesc, &RasterScissorState);

  if(FAILED(hr))
    RDCERR("Failed to create scissoring rasterizer state HRESULT: %s", ToStr(hr).c_str());

  {
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

    FullscreenVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");

    FixedColPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_FixedColPS", "ps_4_0");
    CheckerboardPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_CheckerboardPS", "ps_4_0");
  }
}

void D3D11Replay::GeneralMisc::Release()
{
  SAFE_RELEASE(RasterState);
  SAFE_RELEASE(RasterScissorState);

  SAFE_RELEASE(FullscreenVS);
  SAFE_RELEASE(CheckerboardPS);
  SAFE_RELEASE(FixedColPS);
}

void D3D11Replay::TextureRendering::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  {
    rdcstr hlsl = GetEmbeddedResource(texdisplay_hlsl);

    TexDisplayVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_TexDisplayVS", "vs_4_0");
    TexDisplayPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TexDisplayPS", "ps_5_0");
  }

  {
    rdcstr hlsl = GetEmbeddedResource(texremap_hlsl);

    TexRemapPS[0] = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TexRemapFloat", "ps_5_0");
    TexRemapPS[1] = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TexRemapUInt", "ps_5_0");
    TexRemapPS[2] = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TexRemapSInt", "ps_5_0");
  }

  {
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

    hr = device->CreateBlendState(&blendDesc, &BlendState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create default blendstate HRESULT: %s", ToStr(hr).c_str());
    }
  }

  {
    D3D11_SAMPLER_DESC sampDesc;
    RDCEraseEl(sampDesc);

    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.MaxAnisotropy = 1;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = FLT_MAX;
    sampDesc.MipLODBias = 0.0f;

    hr = device->CreateSamplerState(&sampDesc, &LinearSampState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create linear sampler state HRESULT: %s", ToStr(hr).c_str());
    }

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

    hr = device->CreateSamplerState(&sampDesc, &PointSampState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create point sampler state HRESULT: %s", ToStr(hr).c_str());
    }
  }
}

void D3D11Replay::TextureRendering::Release()
{
  SAFE_RELEASE(PointSampState);
  SAFE_RELEASE(LinearSampState);
  SAFE_RELEASE(BlendState);
  SAFE_RELEASE(TexDisplayVS);
  SAFE_RELEASE(TexDisplayPS);
  for(int i = 0; i < 3; i++)
    SAFE_RELEASE(TexRemapPS[i]);
}

void D3D11Replay::OverlayRendering::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  {
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

    FullscreenVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");

    hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    QuadOverdrawPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS", "ps_5_0");
    QOResolvePS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_QOResolvePS", "ps_5_0");
  }

  {
    rdcstr meshhlsl = GetEmbeddedResource(mesh_hlsl);

    TriangleSizeGS =
        shaderCache->MakeGShader(meshhlsl.c_str(), "RENDERDOC_TriangleSizeGS", "gs_4_0");
    TriangleSizePS =
        shaderCache->MakePShader(meshhlsl.c_str(), "RENDERDOC_TriangleSizePS", "ps_4_0");
  }
  {
    rdcstr hlsl = GetEmbeddedResource(depth_copy_hlsl);

    DepthCopyPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DepthCopyPS", "ps_5_0");
    DepthCopyArrayPS =
        shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DepthCopyArrayPS", "ps_5_0");
    DepthCopyMSPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DepthCopyMSPS", "ps_5_0");
    DepthCopyMSArrayPS =
        shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DepthCopyMSArrayPS", "ps_5_0");
  }
  {
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    HRESULT hr = device->CreateBlendState(&blendDesc, &DepthBlendRTMaskZero);
    if(FAILED(hr))
    {
      RDCERR("Failed to create depth overlay blend state HRESULT: %s", ToStr(hr).c_str());
    }
  }
  {
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilEnable = TRUE;
    dsDesc.StencilReadMask = 0xff;
    dsDesc.StencilWriteMask = 0x0;
    dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
    dsDesc.BackFace = dsDesc.FrontFace;
    HRESULT hr = device->CreateDepthStencilState(&dsDesc, &DepthResolveDS);
    if(FAILED(hr))
    {
      RDCERR("Failed to create depth resolve depth stencil state HRESULT: %s", ToStr(hr).c_str());
    }
  }
}

void D3D11Replay::OverlayRendering::Release()
{
  SAFE_RELEASE(FullscreenVS);
  SAFE_RELEASE(QuadOverdrawPS);
  SAFE_RELEASE(QOResolvePS);
  SAFE_RELEASE(TriangleSizeGS);
  SAFE_RELEASE(TriangleSizePS);
  SAFE_RELEASE(DepthCopyPS);
  SAFE_RELEASE(DepthCopyArrayPS);
  SAFE_RELEASE(DepthCopyMSPS);
  SAFE_RELEASE(DepthCopyMSArrayPS);

  SAFE_RELEASE(DepthResolveDS);
  SAFE_RELEASE(DepthBlendRTMaskZero);

  SAFE_RELEASE(Texture);
}

void D3D11Replay::MeshRendering::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  {
    D3D11_DEPTH_STENCIL_DESC desc;

    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_KEEP;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = desc.StencilWriteMask = 0xff;

    hr = device->CreateDepthStencilState(&desc, &NoDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create no-depth depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    hr = device->CreateDepthStencilState(&desc, &LessEqualDepthState);
  }

  {
    D3D11_RASTERIZER_DESC desc;
    desc.AntialiasedLineEnable = TRUE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.DepthClipEnable = FALSE;
    desc.FrontCounterClockwise = FALSE;
    desc.MultisampleEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.FillMode = D3D11_FILL_WIREFRAME;
    desc.CullMode = D3D11_CULL_NONE;

    hr = device->CreateRasterizerState(&desc, &WireframeRasterState);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersRS HRESULT: %s", ToStr(hr).c_str());

    desc.FrontCounterClockwise = FALSE;
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;

    hr = device->CreateRasterizerState(&desc, &SolidRasterState);
    if(FAILED(hr))
      RDCERR("Failed to create m_SolidHelpersRS HRESULT: %s", ToStr(hr).c_str());
  }

  {
    D3D11_BLEND_DESC desc;
    RDCEraseEl(desc);

    desc.AlphaToCoverageEnable = TRUE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].RenderTargetWriteMask = 0xf;

    hr = device->CreateBlendState(&desc, &WireframeHelpersBS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersRS HRESULT: %s", ToStr(hr).c_str());
  }

  {
    // these elements are just for signature matching sake, so we don't need correct offsets/slots
    D3D11_INPUT_ELEMENT_DESC inputDescSecondary[2] = {
        {"pos", 0, DXGI_FORMAT_R32G32B32A32_FLOAT},
        {"sec", 0, DXGI_FORMAT_R8G8B8A8_UNORM},
    };

    rdcstr meshhlsl = GetEmbeddedResource(mesh_hlsl);

    rdcarray<byte> bytecode;

    MeshVS = shaderCache->MakeVShader(meshhlsl.c_str(), "RENDERDOC_MeshVS", "vs_4_0", 2,
                                      inputDescSecondary, &GenericLayout, &bytecode);
    MeshGS = shaderCache->MakeGShader(meshhlsl.c_str(), "RENDERDOC_MeshGS", "gs_4_0");
    MeshPS = shaderCache->MakePShader(meshhlsl.c_str(), "RENDERDOC_MeshPS", "ps_4_0");

    MeshVSBytecode = new byte[bytecode.size()];
    MeshVSBytelen = (uint32_t)bytecode.size();
    memcpy(MeshVSBytecode, &bytecode[0], bytecode.size());
  }

  {
    Vec4f TLN = Vec4f(-1.0f, 1.0f, 0.0f, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
    Vec4f BLN = Vec4f(-1.0f, -1.0f, 0.0f, 1.0f);
    Vec4f BRN = Vec4f(1.0f, -1.0f, 0.0f, 1.0f);

    Vec4f TLF = Vec4f(-1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f TRF = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f BLF = Vec4f(-1.0f, -1.0f, 1.0f, 1.0f);
    Vec4f BRF = Vec4f(1.0f, -1.0f, 1.0f, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f axisVB[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = axisVB;
    data.SysMemPitch = data.SysMemSlicePitch = 0;

    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.ByteWidth = sizeof(axisVB);
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_IMMUTABLE;

    hr = device->CreateBuffer(&bdesc, &data, &FrustumHelper);

    if(FAILED(hr))
      RDCERR("Failed to create m_FrustumHelper HRESULT: %s", ToStr(hr).c_str());
  }

  {
    Vec4f axisVB[6] = {
        Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
        Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f),
    };

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = axisVB;
    data.SysMemPitch = data.SysMemSlicePitch = 0;

    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.ByteWidth = sizeof(axisVB);
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_IMMUTABLE;

    hr = device->CreateBuffer(&bdesc, &data, &AxisHelper);
    if(FAILED(hr))
      RDCERR("Failed to create m_AxisHelper HRESULT: %s", ToStr(hr).c_str());
  }

  {
    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bdesc.ByteWidth = sizeof(Vec4f) * 24;
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_DYNAMIC;

    hr = device->CreateBuffer(&bdesc, NULL, &TriHighlightHelper);

    if(FAILED(hr))
      RDCERR("Failed to create m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
  }
}

void D3D11Replay::MeshRendering::Release()
{
  SAFE_RELEASE(MeshLayout);

  SAFE_RELEASE(GenericLayout);
  SAFE_RELEASE(NoDepthState);
  SAFE_RELEASE(LessEqualDepthState);

  SAFE_RELEASE(MeshVS);
  SAFE_RELEASE(MeshGS);
  SAFE_RELEASE(MeshPS);

  SAFE_DELETE_ARRAY(MeshVSBytecode);

  SAFE_RELEASE(WireframeRasterState);
  SAFE_RELEASE(WireframeHelpersBS);
  SAFE_RELEASE(SolidRasterState);

  SAFE_RELEASE(FrustumHelper);
  SAFE_RELEASE(AxisHelper);
  SAFE_RELEASE(TriHighlightHelper);
}

void D3D11Replay::VertexPicking::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  rdcstr meshhlsl = GetEmbeddedResource(mesh_hlsl);

  MeshPickCS = shaderCache->MakeCShader(meshhlsl.c_str(), "RENDERDOC_MeshPickCS", "cs_5_0");

  D3D11_BUFFER_DESC bDesc;

  bDesc.ByteWidth = sizeof(Vec4f) * MaxMeshPicks;
  bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  bDesc.CPUAccessFlags = 0;
  bDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  bDesc.StructureByteStride = sizeof(Vec4f);
  bDesc.Usage = D3D11_USAGE_DEFAULT;

  hr = device->CreateBuffer(&bDesc, NULL, &PickResultBuf);

  if(FAILED(hr))
    RDCERR("Failed to create mesh pick result buff HRESULT: %s", ToStr(hr).c_str());

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = MaxMeshPicks;
  uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

  hr = device->CreateUnorderedAccessView(PickResultBuf, &uavDesc, &PickResultUAV);

  if(FAILED(hr))
    RDCERR("Failed to create mesh pick result UAV HRESULT: %s", ToStr(hr).c_str());

  // created/sized on demand
  PickIBBuf = PickVBBuf = NULL;
  PickIBSRV = PickVBSRV = NULL;
  PickIBSize = PickVBSize = 0;
}

void D3D11Replay::VertexPicking::Release()
{
  SAFE_RELEASE(MeshPickCS);
  SAFE_RELEASE(PickIBBuf);
  SAFE_RELEASE(PickVBBuf);
  SAFE_RELEASE(PickIBSRV);
  SAFE_RELEASE(PickVBSRV);
  SAFE_RELEASE(PickResultBuf);
  SAFE_RELEASE(PickResultUAV);
}

void D3D11Replay::PixelPicking::Init(WrappedID3D11Device *device)
{
  HRESULT hr = S_OK;

  {
    D3D11_TEXTURE2D_DESC desc;

    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Width = 100;
    desc.Height = 100;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = 0;

    hr = device->CreateTexture2D(&desc, NULL, &Texture);

    if(FAILED(hr))
    {
      RDCERR("Failed to create pick tex HRESULT: %s", ToStr(hr).c_str());
    }

    if(Texture)
    {
      hr = device->CreateRenderTargetView(Texture, NULL, &RTV);

      if(FAILED(hr))
      {
        RDCERR("Failed to create pick rt HRESULT: %s", ToStr(hr).c_str());
      }
    }
  }

  {
    D3D11_TEXTURE2D_DESC desc;
    RDCEraseEl(desc);
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Width = 1;
    desc.Height = 1;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = device->CreateTexture2D(&desc, NULL, &StageTexture);

    if(FAILED(hr))
    {
      RDCERR("Failed to create pick stage tex HRESULT: %s", ToStr(hr).c_str());
    }
  }
}

void D3D11Replay::PixelPicking::Release()
{
  SAFE_RELEASE(Texture);
  SAFE_RELEASE(RTV);
  SAFE_RELEASE(StageTexture);
}

void ShaderDebugging::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  rdcstr hlsl = GetEmbeddedResource(shaderdebug_hlsl);

  MathCS = shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_DebugMathOp", "cs_5_0");
  SampleVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_DebugSampleVS", "vs_5_0");
  SamplePS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DebugSamplePS", "ps_5_0");

  D3D11_BUFFER_DESC bDesc;

  bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  bDesc.ByteWidth = 6 * sizeof(Vec4f);
  bDesc.CPUAccessFlags = 0;
  bDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  bDesc.StructureByteStride = 6 * sizeof(Vec4f);
  bDesc.Usage = D3D11_USAGE_DEFAULT;

  hr = device->CreateBuffer(&bDesc, NULL, &OutBuf);

  if(FAILED(hr))
    RDCERR("Failed to create shader debugging output buffer HRESULT: %s", ToStr(hr).c_str());

  bDesc.BindFlags = 0;
  bDesc.MiscFlags = 0;
  bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  bDesc.Usage = D3D11_USAGE_STAGING;

  hr = device->CreateBuffer(&bDesc, NULL, &OutStageBuf);

  if(FAILED(hr))
    RDCERR("Failed to create shader debugging staging buffer HRESULT: %s", ToStr(hr).c_str());

  bDesc.ByteWidth = 16 * sizeof(Vec4f);
  bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bDesc.Usage = D3D11_USAGE_DYNAMIC;

  hr = device->CreateBuffer(&bDesc, NULL, &ParamBuf);

  if(FAILED(hr))
    RDCERR("Failed to create shader debugging parameter buffer HRESULT: %s", ToStr(hr).c_str());

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = 1;
  uavDesc.Buffer.Flags = 0;

  if(OutBuf)
    hr = device->CreateUnorderedAccessView(OutBuf, &uavDesc, &OutUAV);

  if(FAILED(hr))
    RDCERR("Failed to create shader debugging UAV HRESULT: %s", ToStr(hr).c_str());

  D3D11_TEXTURE2D_DESC tdesc = {};

  tdesc.ArraySize = 1;
  tdesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  tdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  tdesc.Width = 1;
  tdesc.Height = 1;
  tdesc.SampleDesc.Count = 1;

  hr = device->CreateTexture2D(&tdesc, NULL, &DummyTex);

  if(FAILED(hr))
    RDCERR("Failed to create shader debugging dummy texture HRESULT: %s", ToStr(hr).c_str());

  D3D11_RENDER_TARGET_VIEW_DESC rtDesc;

  rtDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Texture2D.MipSlice = 0;

  if(DummyTex)
    hr = device->CreateRenderTargetView(DummyTex, &rtDesc, &DummyRTV);

  if(FAILED(hr))
    RDCERR("Failed to create shader debugging dummy RTV HRESULT: %s", ToStr(hr).c_str());

  m_pDevice = device;
}

void ShaderDebugging::Release()
{
  SAFE_RELEASE(MathCS);
  SAFE_RELEASE(SampleVS);
  SAFE_RELEASE(SamplePS);
  SAFE_RELEASE(ParamBuf);
  SAFE_RELEASE(OutBuf);
  SAFE_RELEASE(OutStageBuf);
  SAFE_RELEASE(OutUAV);

  SAFE_RELEASE(DummyTex);
  SAFE_RELEASE(DummyRTV);

  for(auto it = m_OffsetSamplePS.begin(); it != m_OffsetSamplePS.end(); ++it)
    it->second->Release();
}

ID3D11PixelShader *ShaderDebugging::GetSamplePS(const int8_t offsets[3])
{
  uint32_t offsKey = offsets[0] | (offsets[1] << 8) | (offsets[2] << 16);
  if(offsKey == 0)
    return SamplePS;

  ID3D11PixelShader *ps = m_OffsetSamplePS[offsKey];
  if(ps)
    return ps;

  D3D11ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  shaderCache->SetCaching(true);

  rdcstr hlsl = GetEmbeddedResource(shaderdebug_hlsl);

  hlsl = StringFormat::Fmt("#define debugSampleOffsets int4(%d,%d,%d,0)\n\n%s", offsets[0],
                           offsets[1], offsets[2], hlsl.c_str());

  ps = m_OffsetSamplePS[offsKey] =
      shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_DebugSamplePS", "ps_5_0");

  shaderCache->SetCaching(false);

  return ps;
}

void D3D11Replay::HistogramMinMax::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  const uint32_t maxTexDim = 16384;
  const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
  const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

  D3D11_BUFFER_DESC bDesc;

  bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
  bDesc.ByteWidth =
      2 * 4 * sizeof(float) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;
  bDesc.CPUAccessFlags = 0;
  bDesc.MiscFlags = 0;
  bDesc.StructureByteStride = 0;
  bDesc.Usage = D3D11_USAGE_DEFAULT;

  hr = device->CreateBuffer(&bDesc, NULL, &TileResultBuff);

  if(FAILED(hr))
  {
    RDCERR("Failed to create tile result buffer HRESULT: %s", ToStr(hr).c_str());
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
  srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  srvDesc.Buffer.FirstElement = 0;
  srvDesc.Buffer.NumElements = bDesc.ByteWidth / sizeof(Vec4f);

  hr = device->CreateShaderResourceView(TileResultBuff, &srvDesc, &TileResultSRV[0]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result SRV 0 HRESULT: %s", ToStr(hr).c_str());

  srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  hr = device->CreateShaderResourceView(TileResultBuff, &srvDesc, &TileResultSRV[1]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result SRV 1 HRESULT: %s", ToStr(hr).c_str());

  srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  hr = device->CreateShaderResourceView(TileResultBuff, &srvDesc, &TileResultSRV[2]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result SRV 2 HRESULT: %s", ToStr(hr).c_str());

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.Flags = 0;
  uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements;

  hr = device->CreateUnorderedAccessView(TileResultBuff, &uavDesc, &TileResultUAV[0]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result UAV 0 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  hr = device->CreateUnorderedAccessView(TileResultBuff, &uavDesc, &TileResultUAV[1]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result UAV 1 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  hr = device->CreateUnorderedAccessView(TileResultBuff, &uavDesc, &TileResultUAV[2]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result UAV 2 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32_UINT;
  uavDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
  bDesc.ByteWidth = uavDesc.Buffer.NumElements * sizeof(int);

  hr = device->CreateBuffer(&bDesc, NULL, &ResultBuff);

  if(FAILED(hr))
    RDCERR("Failed to create histogram buff HRESULT: %s", ToStr(hr).c_str());

  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &HistogramUAV);

  if(FAILED(hr))
    RDCERR("Failed to create histogram UAV HRESULT: %s", ToStr(hr).c_str());

  bDesc.BindFlags = 0;
  bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  bDesc.Usage = D3D11_USAGE_STAGING;

  hr = device->CreateBuffer(&bDesc, NULL, &ResultStageBuff);

  if(FAILED(hr))
    RDCERR("Failed to create histogram stage buff HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  uavDesc.Buffer.NumElements = 2;

  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &ResultUAV[0]);

  if(FAILED(hr))
    RDCERR("Failed to create result UAV 0 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &ResultUAV[1]);

  if(FAILED(hr))
    RDCERR("Failed to create result UAV 1 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &ResultUAV[2]);

  if(FAILED(hr))
    RDCERR("Failed to create result UAV 2 HRESULT: %s", ToStr(hr).c_str());

  rdcstr histogramhlsl = GetEmbeddedResource(histogram_hlsl);

  for(int t = eTexType_1D; t < eTexType_Max; t++)
  {
    if(t == eTexType_Unused)
      continue;

    // float, uint, sint
    for(int i = 0; i < 3; i++)
    {
      rdcstr hlsl = rdcstr("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
      hlsl += rdcstr("#define SHADER_BASETYPE ") + ToStr(i) + "\n";
      hlsl += histogramhlsl;

      TileMinMaxCS[t][i] =
          shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_TileMinMaxCS", "cs_5_0");
      HistogramCS[t][i] = shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_HistogramCS", "cs_5_0");

      if(t == 1)
        ResultMinMaxCS[i] =
            shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS", "cs_5_0");
    }
  }
}

void D3D11Replay::HistogramMinMax::Release()
{
  SAFE_RELEASE(TileResultBuff);
  SAFE_RELEASE(ResultBuff);
  SAFE_RELEASE(ResultStageBuff);

  for(int i = 0; i < 3; i++)
  {
    SAFE_RELEASE(TileResultUAV[i]);
    SAFE_RELEASE(ResultUAV[i]);
    SAFE_RELEASE(TileResultSRV[i]);
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

  SAFE_RELEASE(HistogramUAV);
}

void D3D11Replay::PixelHistory::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  {
    D3D11_BLEND_DESC blendDesc = {};

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;

    hr = device->CreateBlendState(&blendDesc, &NopBlendState);
  }

  {
    D3D11_DEPTH_STENCIL_DESC desc;

    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_KEEP;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilReadMask = desc.StencilWriteMask = 0;
    desc.StencilEnable = FALSE;

    hr = device->CreateDepthStencilState(&desc, &NopDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create nop depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.StencilEnable = TRUE;
    desc.StencilReadMask = desc.StencilWriteMask = 0xff;

    hr = device->CreateDepthStencilState(&desc, &AllPassDepthState);

    if(FAILED(hr))
      RDCERR("Failed to create always pass depthstencilstate HRESULT: %s", ToStr(hr).c_str());

    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_INCR_SAT;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    hr = device->CreateDepthStencilState(&desc, &AllPassIncrDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate HRESULT: %s",
             ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

    hr = device->CreateDepthStencilState(&desc, &StencIncrEqDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate HRESULT: %s",
             ToStr(hr).c_str());
    }
  }

  {
    rdcstr hlsl = GetEmbeddedResource(pixelhistory_hlsl);

    PrimitiveIDPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_PrimitiveIDPS", "ps_5_0");
  }
}

void D3D11Replay::PixelHistory::Release()
{
  SAFE_RELEASE(NopBlendState);
  SAFE_RELEASE(NopDepthState);
  SAFE_RELEASE(AllPassDepthState);
  SAFE_RELEASE(AllPassIncrDepthState);
  SAFE_RELEASE(StencIncrEqDepthState);
  SAFE_RELEASE(PrimitiveIDPS);
}
