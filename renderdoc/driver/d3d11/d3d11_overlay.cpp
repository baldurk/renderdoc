/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "data/resource.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_manager.h"

#include "data/hlsl/hlsl_cbuffers.h"

ResourceId D3D11Replay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventId, const std::vector<uint32_t> &passEvents)
{
  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(texid, typeHint, false);

  D3D11MarkerRegion renderoverlay(StringFormat::Fmt("RenderOverlay %d", overlay));

  ResourceId id = texid;

  D3D11_TEXTURE2D_DESC realTexDesc;
  realTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  realTexDesc.Usage = D3D11_USAGE_DEFAULT;
  realTexDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  realTexDesc.ArraySize = 1;
  realTexDesc.MipLevels = 1;
  realTexDesc.CPUAccessFlags = 0;
  realTexDesc.MiscFlags = 0;
  realTexDesc.SampleDesc.Count = 1;
  realTexDesc.SampleDesc.Quality = 0;
  realTexDesc.Width = details.texWidth;
  realTexDesc.Height = details.texHeight;

  if(details.texType == eTexType_2DMS || details.texType == eTexType_DepthMS ||
     details.texType == eTexType_StencilMS)
  {
    realTexDesc.SampleDesc.Count = details.sampleCount;
    realTexDesc.SampleDesc.Quality = details.sampleQuality;
  }

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  D3D11_TEXTURE2D_DESC customTexDesc;
  RDCEraseEl(customTexDesc);
  if(m_Overlay.Texture)
    m_Overlay.Texture->GetDesc(&customTexDesc);

  WrappedID3D11Texture2D1 *wrappedCustomRenderTex = (WrappedID3D11Texture2D1 *)m_Overlay.Texture;

  // need to recreate backing custom render tex
  if(realTexDesc.Width != customTexDesc.Width || realTexDesc.Height != customTexDesc.Height ||
     realTexDesc.Format != customTexDesc.Format ||
     realTexDesc.SampleDesc.Count != customTexDesc.SampleDesc.Count ||
     realTexDesc.SampleDesc.Quality != customTexDesc.SampleDesc.Quality)
  {
    SAFE_RELEASE(m_Overlay.Texture);
    m_Overlay.resourceId = ResourceId();

    ID3D11Texture2D *customRenderTex = NULL;
    HRESULT hr = m_pDevice->CreateTexture2D(&realTexDesc, NULL, &customRenderTex);
    if(FAILED(hr))
    {
      RDCERR("Failed to create custom render tex HRESULT: %s", ToStr(hr).c_str());
      return ResourceId();
    }
    wrappedCustomRenderTex = (WrappedID3D11Texture2D1 *)customRenderTex;

    m_Overlay.Texture = wrappedCustomRenderTex;
    m_Overlay.resourceId = wrappedCustomRenderTex->GetResourceID();
  }

  ID3D11Texture2D *preDrawDepth = NULL;
  ID3D11Texture2D *renderDepth = NULL;

  ID3D11DepthStencilView *dsView = NULL;

  m_pImmediateContext->OMGetRenderTargets(0, NULL, &dsView);

  D3D11_DEPTH_STENCIL_VIEW_DESC dsViewDesc;
  RDCEraseEl(dsViewDesc);
  if(dsView)
  {
    ID3D11Texture2D *realDepth = NULL;

    dsView->GetResource((ID3D11Resource **)&realDepth);

    dsView->GetDesc(&dsViewDesc);

    SAFE_RELEASE(dsView);

    D3D11_TEXTURE2D_DESC desc;

    realDepth->GetDesc(&desc);

    HRESULT hr = S_OK;

    hr = m_pDevice->CreateTexture2D(&desc, NULL, &preDrawDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create preDrawDepth HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(realDepth);
      return m_Overlay.resourceId;
    }
    hr = m_pDevice->CreateTexture2D(&desc, NULL, &renderDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(realDepth);
      return m_Overlay.resourceId;
    }

    m_pImmediateContext->CopyResource(preDrawDepth, realDepth);

    SAFE_RELEASE(realDepth);
  }

  D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
  rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  rtDesc.Texture2D.MipSlice = 0;

  if(realTexDesc.SampleDesc.Count > 1 || realTexDesc.SampleDesc.Quality > 0)
  {
    rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
  }

  ID3D11RenderTargetView *rtv = NULL;
  HRESULT hr = m_pDevice->CreateRenderTargetView(wrappedCustomRenderTex, &rtDesc, &rtv);
  if(FAILED(hr))
  {
    RDCERR("Failed to create custom render tex RTV HRESULT: %s", ToStr(hr).c_str());
    return m_Overlay.resourceId;
  }

  FLOAT black[] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_pImmediateContext->ClearRenderTargetView(rtv, black);

  if(renderDepth)
  {
    m_pImmediateContext->CopyResource(renderDepth, preDrawDepth);

    hr = m_pDevice->CreateDepthStencilView(renderDepth, &dsViewDesc, &dsView);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth DSV HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }
  }

  m_pImmediateContext->OMSetRenderTargets(1, &rtv, dsView);

  SAFE_RELEASE(dsView);

  D3D11_DEPTH_STENCIL_DESC dsDesc;

  dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
      dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
      dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  dsDesc.DepthEnable = TRUE;
  dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  dsDesc.StencilEnable = FALSE;
  dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create drawcall depth stencil state HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      rdesc.FillMode = D3D11_FILL_SOLID;
      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.FrontCounterClockwise = FALSE;
      rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
      rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
      rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rdesc.DepthClipEnable = FALSE;
      rdesc.ScissorEnable = FALSE;
      rdesc.MultisampleEnable = FALSE;
      rdesc.AntialiasedLineEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }
    }

    float clearColour[] = {0.0f, 0.0f, 0.0f, 0.5f};
    m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

    float overlayConsts[] = {0.8f, 0.1f, 0.8f, 1.0f};
    ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rs);

    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create drawcall depth stencil state HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    ID3D11RasterizerState *rsCull = NULL;
    D3D11_RASTERIZER_DESC origdesc;

    {
      m_pImmediateContext->RSGetState(&rs);

      if(rs)
      {
        rs->GetDesc(&origdesc);
      }
      else
      {
        origdesc.CullMode = D3D11_CULL_BACK;
        origdesc.FrontCounterClockwise = FALSE;
      }

      SAFE_RELEASE(rs);
    }

    {
      D3D11_RASTERIZER_DESC rdesc;

      rdesc.FillMode = D3D11_FILL_SOLID;
      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.FrontCounterClockwise = origdesc.FrontCounterClockwise;
      rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
      rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
      rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rdesc.DepthClipEnable = FALSE;
      rdesc.ScissorEnable = FALSE;
      rdesc.MultisampleEnable = FALSE;
      rdesc.AntialiasedLineEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }

      rdesc.CullMode = origdesc.CullMode;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rsCull);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }
    }

    float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

    float overlayConsts[] = {1.0f, 0.0f, 0.0f, 1.0f};
    ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rs);

    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    overlayConsts[0] = 0.0f;
    overlayConsts[1] = 1.0f;

    buf = GetDebugManager()->MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rsCull);

    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
    SAFE_RELEASE(rsCull);
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    m_pImmediateContext->VSSetShader(m_Overlay.FullscreenVS, NULL, 0);
    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    m_pImmediateContext->PSSetShader(m_General.CheckerboardPS, NULL, 0);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImmediateContext->IASetInputLayout(NULL);

    D3D11_RASTERIZER_DESC origdesc;

    {
      ID3D11RasterizerState *rs = NULL;

      m_pImmediateContext->RSGetState(&rs);

      if(rs)
        rs->GetDesc(&origdesc);
      else
        origdesc.ScissorEnable = FALSE;

      SAFE_RELEASE(rs);
    }

    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create drawcall depth stencil state HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    D3D11_BLEND_DESC blendDesc;
    RDCEraseEl(blendDesc);

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState *bs = NULL;
    hr = m_pDevice->CreateBlendState(&blendDesc, &bs);

    float blendwhite[] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_pImmediateContext->OMSetBlendState(bs, blendwhite, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      rdesc.FillMode = D3D11_FILL_SOLID;
      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.FrontCounterClockwise = FALSE;
      rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
      rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
      rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rdesc.DepthClipEnable = FALSE;
      rdesc.ScissorEnable = FALSE;
      rdesc.MultisampleEnable = FALSE;
      rdesc.AntialiasedLineEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }
    }

    float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

    m_pImmediateContext->RSSetState(rs);

    CheckerboardCBuffer pixelData = {0};

    UINT dummy = 1;
    D3D11_VIEWPORT views[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {0};
    m_pImmediateContext->RSGetViewports(&dummy, views);

    pixelData.BorderWidth = 3;
    pixelData.CheckerSquareDimension = 16.0f;

    // set primary/secondary to the same to 'disable' checkerboard
    pixelData.PrimaryColor = pixelData.SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
    pixelData.InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);

    // set viewport rect
    pixelData.RectPosition = Vec2f(views[0].TopLeftX, views[0].TopLeftY);
    pixelData.RectSize = Vec2f(views[0].Width, views[0].Height);

    ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(&pixelData, sizeof(pixelData));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->Draw(3, 0);

    if(origdesc.ScissorEnable)
    {
      D3D11_RECT rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {0};
      m_pImmediateContext->RSGetScissorRects(&dummy, rects);

      D3D11_VIEWPORT scissorview;

      scissorview.TopLeftX = (float)rects[0].left;
      scissorview.TopLeftY = (float)rects[0].top;
      scissorview.MinDepth = 0.0f;
      scissorview.MaxDepth = 1.0f;
      scissorview.Width = (float)(rects[0].right - rects[0].left);
      scissorview.Height = (float)(rects[0].bottom - rects[0].top);

      m_pImmediateContext->RSSetViewports(1, &scissorview);

      // black/white checkered border
      pixelData.PrimaryColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
      pixelData.SecondaryColor = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

      // nothing at all inside
      pixelData.InnerColor = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

      // set scissor rect
      pixelData.RectPosition = Vec2f(scissorview.TopLeftX, scissorview.TopLeftY);
      pixelData.RectSize = Vec2f(scissorview.Width, scissorview.Height);

      buf = GetDebugManager()->MakeCBuffer(&pixelData, sizeof(pixelData));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->Draw(3, 0);
    }

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
    SAFE_RELEASE(bs);
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

    dsDesc.DepthEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create wireframe depth state HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      m_pImmediateContext->RSGetState(&rs);

      if(rs)
      {
        rs->GetDesc(&rdesc);
      }
      else
      {
        rdesc.FillMode = D3D11_FILL_SOLID;
        rdesc.CullMode = D3D11_CULL_BACK;
        rdesc.FrontCounterClockwise = FALSE;
        rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
        rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rdesc.DepthClipEnable = TRUE;
        rdesc.ScissorEnable = FALSE;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = FALSE;
      }

      SAFE_RELEASE(rs);

      rdesc.FillMode = D3D11_FILL_WIREFRAME;
      rdesc.CullMode = D3D11_CULL_NONE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create wireframe rast state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }
    }

    float overlayConsts[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, overlayConsts);

    overlayConsts[3] = 1.0f;
    ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rs);

    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
  }
  else if(overlay == DebugOverlay::ClearBeforePass || overlay == DebugOverlay::ClearBeforeDraw)
  {
    std::vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }

      const D3D11RenderState &state = tracker.State();

      if(overlay == DebugOverlay::ClearBeforeDraw)
      {
        UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1,
                                                       (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};

        if(m_pImmediateContext->IsFL11_1())
          m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
              RDCMIN(state.OM.UAVStartSlot, (UINT)D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT),
              state.OM.RenderTargets, state.OM.DepthView, state.OM.UAVStartSlot,
              D3D11_1_UAV_SLOT_COUNT - state.OM.UAVStartSlot, state.OM.UAVs, UAV_keepcounts);
        else
          m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
              RDCMIN(state.OM.UAVStartSlot, (UINT)D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT),
              state.OM.RenderTargets, state.OM.DepthView, state.OM.UAVStartSlot,
              D3D11_PS_CS_UAV_REGISTER_COUNT - state.OM.UAVStartSlot, state.OM.UAVs, UAV_keepcounts);
      }

      for(size_t i = 0; i < ARRAY_COUNT(state.OM.RenderTargets); i++)
        if(state.OM.RenderTargets[i])
          m_pImmediateContext->ClearRenderTargetView(state.OM.RenderTargets[i], black);

      for(size_t i = 0; i < events.size(); i++)
      {
        m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass)
        {
          m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_pDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    SCOPED_TIMER("Triangle size");

    // ensure it will be recreated on next use
    SAFE_RELEASE(m_MeshRender.MeshLayout);
    m_MeshRender.PrevPositionFormat = ResourceFormat();

    D3D11_INPUT_ELEMENT_DESC layoutdesc[2] = {};

    layoutdesc[0].SemanticName = "pos";
    layoutdesc[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    // dummy for vertex shader
    layoutdesc[1].SemanticName = "sec";
    layoutdesc[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    layoutdesc[1].InputSlot = 1;
    layoutdesc[1].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;

    hr = m_pDevice->CreateInputLayout(layoutdesc, 2, m_MeshRender.MeshVSBytecode,
                                      m_MeshRender.MeshVSBytelen, &m_MeshRender.MeshLayout);

    if(FAILED(hr))
    {
      RDCERR("Failed to create m_MeshRender.m_MeshDisplayLayout HRESULT: %s", ToStr(hr).c_str());
      m_MeshRender.MeshLayout = NULL;
    }

    MeshVertexCBuffer vertexData = {};
    vertexData.ModelViewProj = Matrix4f::Identity();
    vertexData.SpriteSize = Vec2f();
    ID3D11Buffer *vsBuf = GetDebugManager()->MakeCBuffer(&vertexData, sizeof(vertexData));

    float overlayConsts[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, overlayConsts);

    std::vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::TriangleSizeDraw)
      events.clear();

    events.push_back(eventId);

    if(overlay == DebugOverlay::TriangleSizePass)
      m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

    events.push_back(eventId);

    D3D11_VIEWPORT view = m_pImmediateContext->GetCurrentPipelineState()->RS.Viewports[0];

    Vec4f viewport = Vec4f(view.Width, view.Height);
    ID3D11Buffer *gsbuf = GetDebugManager()->MakeCBuffer(&viewport.x, sizeof(viewport));

    for(size_t i = 0; i < events.size(); i++)
    {
      D3D11RenderState oldstate = *m_pImmediateContext->GetCurrentPipelineState();

      D3D11_DEPTH_STENCIL_DESC dsdesc = {
          /*DepthEnable =*/TRUE,
          /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
          /*DepthFunc =*/D3D11_COMPARISON_LESS,
          /*StencilEnable =*/FALSE,
          /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
          /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
          /*FrontFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                          D3D11_COMPARISON_ALWAYS},
          /*BackFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                         D3D11_COMPARISON_ALWAYS},
      };
      ID3D11DepthStencilState *ds = NULL;

      if(oldstate.OM.DepthStencilState)
        oldstate.OM.DepthStencilState->GetDesc(&dsdesc);

      dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
      dsdesc.StencilWriteMask = 0;

      m_pDevice->CreateDepthStencilState(&dsdesc, &ds);

      m_pImmediateContext->OMSetDepthStencilState(ds, oldstate.OM.StencRef);

      SAFE_RELEASE(ds);

      const DrawcallDescription *draw = m_pDevice->GetDrawcall(events[i]);

      for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
      {
        MeshFormat fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::GSOut);
        if(fmt.vertexResourceId == ResourceId())
          fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::VSOut);

        if(fmt.vertexResourceId != ResourceId())
        {
          D3D11_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(fmt.topology);

          ID3D11Buffer *ibuf = NULL;
          DXGI_FORMAT ifmt = DXGI_FORMAT_R16_UINT;
          UINT ioffs = (UINT)fmt.indexByteOffset;

          ID3D11Buffer *vbs[2] = {NULL, NULL};
          UINT str[2] = {fmt.vertexByteStride, 4};
          UINT offs[2] = {(UINT)fmt.vertexByteOffset, 0};

          {
            auto it = WrappedID3D11Buffer::m_BufferList.find(fmt.vertexResourceId);

            if(it != WrappedID3D11Buffer::m_BufferList.end())
              vbs[0] = it->second.m_Buffer;

            it = WrappedID3D11Buffer::m_BufferList.find(fmt.indexResourceId);

            if(it != WrappedID3D11Buffer::m_BufferList.end())
              ibuf = it->second.m_Buffer;

            if(fmt.indexByteStride == 4)
              ifmt = DXGI_FORMAT_R32_UINT;
          }

          m_pImmediateContext->IASetVertexBuffers(0, 1, vbs, str, offs);
          if(ibuf)
            m_pImmediateContext->IASetIndexBuffer(ibuf, ifmt, ioffs);
          else
            m_pImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, NULL);

          m_pImmediateContext->IASetPrimitiveTopology(topo);

          m_pImmediateContext->IASetInputLayout(m_MeshRender.MeshLayout);
          m_pImmediateContext->VSSetConstantBuffers(0, 1, &vsBuf);
          m_pImmediateContext->GSSetConstantBuffers(0, 1, &gsbuf);
          m_pImmediateContext->VSSetShader(m_MeshRender.MeshVS, NULL, 0);
          m_pImmediateContext->GSSetShader(m_Overlay.TriangleSizeGS, NULL, 0);
          m_pImmediateContext->PSSetShader(m_Overlay.TriangleSizePS, NULL, 0);
          m_pImmediateContext->HSSetShader(NULL, NULL, 0);
          m_pImmediateContext->DSSetShader(NULL, NULL, 0);
          m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);
          m_pImmediateContext->OMSetRenderTargets(1, &rtv, oldstate.OM.DepthView);

          if(ibuf)
            m_pImmediateContext->DrawIndexed(fmt.numIndices, 0, fmt.baseVertex);
          else
            m_pImmediateContext->Draw(fmt.numIndices, 0);
        }
      }

      oldstate.ApplyState(m_pImmediateContext);

      if(overlay == DebugOverlay::TriangleSizePass)
      {
        m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(i + 1 < events.size())
          m_pDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
      }
    }

    if(overlay == DebugOverlay::TriangleSizePass)
      m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    SCOPED_TIMER("Quad Overdraw");

    std::vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::QuadOverdrawDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::QuadOverdrawPass)
        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      D3D11RenderState *state = m_pImmediateContext->GetCurrentPipelineState();

      uint32_t width = 1920 >> 1;
      uint32_t height = 1080 >> 1;

      D3D11_TEXTURE2D_DESC overrideDepthDesc = {};
      ID3D11Texture2D *depthTex = NULL;

      {
        ID3D11Resource *res = NULL;
        if(state->OM.DepthView)
        {
          state->OM.DepthView->GetResource(&res);
        }
        else if(state->OM.RenderTargets[0])
        {
          state->OM.RenderTargets[0]->GetResource(&res);
        }
        else
        {
          RDCERR("Couldn't get size of existing targets");
          return m_Overlay.resourceId;
        }

        D3D11_RESOURCE_DIMENSION dim;
        res->GetType(&dim);

        if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
        {
          D3D11_TEXTURE1D_DESC texdesc;
          ((ID3D11Texture1D *)res)->GetDesc(&texdesc);

          width = RDCMAX(1U, texdesc.Width >> 1);
          height = 1;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
          D3D11_TEXTURE2D_DESC texdesc;
          ((ID3D11Texture2D *)res)->GetDesc(&texdesc);

          width = RDCMAX(1U, texdesc.Width >> 1);
          height = RDCMAX(1U, texdesc.Height >> 1);

          if(state->OM.DepthView && texdesc.SampleDesc.Count > 1)
          {
            overrideDepthDesc = texdesc;
            overrideDepthDesc.ArraySize = texdesc.SampleDesc.Count;
            overrideDepthDesc.SampleDesc.Count = 1;
            depthTex = ((ID3D11Texture2D *)res);

            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            state->OM.DepthView->GetDesc(&dsvDesc);

            // bake in any view format cast
            if(dsvDesc.Format != DXGI_FORMAT_UNKNOWN && dsvDesc.Format != overrideDepthDesc.Format)
              overrideDepthDesc.Format = dsvDesc.Format;

            // only need depth stencil, and other bind flags may be invalid with this typed format
            overrideDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            overrideDepthDesc.MiscFlags = 0;

            RDCASSERT(!IsTypelessFormat(overrideDepthDesc.Format), overrideDepthDesc.Format);
          }
        }
        else
        {
          RDCERR("Trying to show quad overdraw on invalid view");
          return m_Overlay.resourceId;
        }

        SAFE_RELEASE(res);
      }

      ID3D11DepthStencilView *depthOverride = NULL;

      if(overrideDepthDesc.Width > 0)
      {
        ID3D11Texture2D *tex = NULL;
        m_pDevice->CreateTexture2D(&overrideDepthDesc, NULL, &tex);

        D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
        viewDesc.Format = overrideDepthDesc.Format;
        viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.ArraySize = 1;

        m_pDevice->GetDebugManager()->CopyTex2DMSToArray(UNWRAP(WrappedID3D11Texture2D1, tex),
                                                         UNWRAP(WrappedID3D11Texture2D1, depthTex));

        m_pDevice->CreateDepthStencilView(tex, &viewDesc, &depthOverride);
        SAFE_RELEASE(tex);
      }

      D3D11_TEXTURE2D_DESC uavTexDesc = {
          width,
          height,
          1U,
          4U,
          DXGI_FORMAT_R32_UINT,
          {1, 0},
          D3D11_USAGE_DEFAULT,
          D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
          0,
          0,
      };

      ID3D11Texture2D *overdrawTex = NULL;
      ID3D11ShaderResourceView *overdrawSRV = NULL;
      ID3D11UnorderedAccessView *overdrawUAV = NULL;

      m_pDevice->CreateTexture2D(&uavTexDesc, NULL, &overdrawTex);
      m_pDevice->CreateShaderResourceView(overdrawTex, NULL, &overdrawSRV);
      m_pDevice->CreateUnorderedAccessView(overdrawTex, NULL, &overdrawUAV);

      UINT vals[4] = {};
      m_pImmediateContext->ClearUnorderedAccessViewUint(overdrawUAV, vals);

      for(size_t i = 0; i < events.size(); i++)
      {
        D3D11RenderState oldstate = *m_pImmediateContext->GetCurrentPipelineState();

        {
          D3D11_DEPTH_STENCIL_DESC dsdesc = {
              /*DepthEnable =*/TRUE,
              /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
              /*DepthFunc =*/D3D11_COMPARISON_LESS,
              /*StencilEnable =*/FALSE,
              /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
              /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
              /*FrontFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                              D3D11_COMPARISON_ALWAYS},
              /*BackFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                             D3D11_COMPARISON_ALWAYS},
          };
          ID3D11DepthStencilState *ds = NULL;

          if(state->OM.DepthStencilState)
            state->OM.DepthStencilState->GetDesc(&dsdesc);

          dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
          dsdesc.StencilWriteMask = 0;

          m_pDevice->CreateDepthStencilState(&dsdesc, &ds);

          m_pImmediateContext->OMSetDepthStencilState(ds, oldstate.OM.StencRef);

          SAFE_RELEASE(ds);
        }

        {
          D3D11_RASTERIZER_DESC rdesc;
          ID3D11RasterizerState *rs = NULL;

          if(state->RS.State)
          {
            state->RS.State->GetDesc(&rdesc);
          }
          else
          {
            rdesc.FillMode = D3D11_FILL_SOLID;
            rdesc.CullMode = D3D11_CULL_BACK;
            rdesc.FrontCounterClockwise = FALSE;
            rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
            rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
            rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            rdesc.DepthClipEnable = TRUE;
            rdesc.ScissorEnable = FALSE;
            rdesc.MultisampleEnable = FALSE;
            rdesc.AntialiasedLineEnable = FALSE;
          }

          rdesc.MultisampleEnable = FALSE;

          m_pDevice->CreateRasterizerState(&rdesc, &rs);

          m_pImmediateContext->RSSetState(rs);

          SAFE_RELEASE(rs);
        }

        UINT UAVcount = 0;
        m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
            0, NULL, depthOverride ? depthOverride : oldstate.OM.DepthView, 0, 1, &overdrawUAV,
            &UAVcount);

        m_pImmediateContext->PSSetShader(m_Overlay.QuadOverdrawPS, NULL, 0);

        m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        oldstate.ApplyState(m_pImmediateContext);

        if(overlay == DebugOverlay::QuadOverdrawPass)
        {
          m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_pDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }

      SAFE_RELEASE(depthOverride);

      // resolve pass
      {
        m_pImmediateContext->VSSetShader(m_Overlay.FullscreenVS, NULL, 0);
        m_pImmediateContext->HSSetShader(NULL, NULL, 0);
        m_pImmediateContext->DSSetShader(NULL, NULL, 0);
        m_pImmediateContext->GSSetShader(NULL, NULL, 0);
        m_pImmediateContext->PSSetShader(m_Overlay.QOResolvePS, NULL, 0);
        m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pImmediateContext->IASetInputLayout(NULL);

        m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);

        m_pImmediateContext->OMSetDepthStencilState(NULL, 0);
        m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);
        m_pImmediateContext->RSSetState(m_General.RasterState);

        float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

        m_pImmediateContext->PSSetShaderResources(0, 1, &overdrawSRV);

        m_pImmediateContext->Draw(3, 0);
      }

      SAFE_RELEASE(overdrawTex);
      SAFE_RELEASE(overdrawSRV);
      SAFE_RELEASE(overdrawUAV);

      if(overlay == DebugOverlay::QuadOverdrawPass)
        m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);
    }
  }
  else if(preDrawDepth)
  {
    D3D11_DEPTH_STENCIL_DESC cur = {0};

    UINT stencilRef = 0;

    {
      ID3D11DepthStencilState *os = NULL;
      m_pImmediateContext->OMGetDepthStencilState(&os, &stencilRef);

      if(os)
      {
        os->GetDesc(&cur);
        SAFE_RELEASE(os);
      }
      else
      {
        cur.DepthEnable = TRUE;
        cur.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        cur.DepthFunc = D3D11_COMPARISON_LESS;    // default depth func
        cur.StencilEnable = FALSE;
        cur.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
        cur.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
        cur.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        cur.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        cur.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        cur.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        cur.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        cur.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        cur.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        cur.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
      }
    }

    // make sure that if a test is disabled, it shows all
    // pixels passing
    if(!cur.DepthEnable)
      cur.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if(!cur.StencilEnable)
      cur.StencilEnable = D3D11_COMPARISON_ALWAYS;

    // ensure culling/depth clipping doesn't hide the render for the fail draw
    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      m_pImmediateContext->RSGetState(&rs);

      if(rs)
      {
        rs->GetDesc(&rdesc);
      }
      else
      {
        rdesc.FillMode = D3D11_FILL_SOLID;
        rdesc.CullMode = D3D11_CULL_BACK;
        rdesc.FrontCounterClockwise = FALSE;
        rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
        rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rdesc.DepthClipEnable = TRUE;
        rdesc.ScissorEnable = FALSE;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = FALSE;
      }

      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.DepthClipEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create wireframe rast state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }
    }

    if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
    {
      ID3D11DepthStencilState *os = NULL;

      D3D11_DEPTH_STENCIL_DESC d = dsDesc;

      if(overlay == DebugOverlay::Depth)
      {
        dsDesc.DepthEnable = d.DepthEnable = TRUE;
        dsDesc.StencilEnable = d.StencilEnable = FALSE;

        d.DepthFunc = D3D11_COMPARISON_ALWAYS;
      }
      else if(overlay == DebugOverlay::Stencil)
      {
        dsDesc.DepthEnable = d.DepthEnable = FALSE;
        dsDesc.StencilEnable = d.StencilEnable = TRUE;

        d.FrontFace = cur.FrontFace;
        d.BackFace = cur.BackFace;
        dsDesc.StencilReadMask = d.StencilReadMask = cur.StencilReadMask;
        dsDesc.StencilWriteMask = d.StencilWriteMask = cur.StencilWriteMask;

        d.BackFace.StencilFunc = d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
      }

      SAFE_RELEASE(os);
      hr = m_pDevice->CreateDepthStencilState(&d, &os);
      if(FAILED(hr))
      {
        RDCERR("Failed to create depth/stencil overlay depth state HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(rs);
        return m_Overlay.resourceId;
      }

      m_pImmediateContext->OMSetDepthStencilState(os, stencilRef);

      m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

      float redConsts[] = {255.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f};

      ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(redConsts, sizeof(redConsts));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

      ID3D11RasterizerState *prevrs = NULL;
      m_pImmediateContext->RSGetState(&prevrs);

      m_pImmediateContext->RSSetState(rs);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      m_pImmediateContext->RSSetState(prevrs);

      SAFE_RELEASE(os);

      m_pImmediateContext->CopyResource(renderDepth, preDrawDepth);

      d = dsDesc;

      if(overlay == DebugOverlay::Depth)
      {
        d.DepthFunc = cur.DepthFunc;
      }
      else if(overlay == DebugOverlay::Stencil)
      {
        d.FrontFace = cur.FrontFace;
        d.BackFace = cur.BackFace;
      }

      hr = m_pDevice->CreateDepthStencilState(&d, &os);
      if(FAILED(hr))
      {
        RDCERR("Failed to create depth/stencil overlay depth state 2 HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }

      m_pImmediateContext->OMSetDepthStencilState(os, stencilRef);

      float greenConsts[] = {0.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f};

      buf = GetDebugManager()->MakeCBuffer(greenConsts, sizeof(greenConsts));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      SAFE_RELEASE(os);
    }

    SAFE_RELEASE(rs);
  }
  else
  {
    // no depth? trivial pass for depth or stencil tests
    if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
    {
      m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

      dsDesc.DepthEnable = FALSE;
      dsDesc.StencilEnable = FALSE;

      ID3D11DepthStencilState *os = NULL;
      hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall depth stencil state HRESULT: %s", ToStr(hr).c_str());
        return m_Overlay.resourceId;
      }

      m_pImmediateContext->OMSetDepthStencilState(os, 0);

      m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

      ID3D11RasterizerState *rs = NULL;
      {
        D3D11_RASTERIZER_DESC rdesc;

        rdesc.FillMode = D3D11_FILL_SOLID;
        rdesc.CullMode = D3D11_CULL_NONE;
        rdesc.FrontCounterClockwise = FALSE;
        rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
        rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rdesc.DepthClipEnable = FALSE;
        rdesc.ScissorEnable = FALSE;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = FALSE;

        hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
        if(FAILED(hr))
        {
          RDCERR("Failed to create drawcall rast state HRESULT: %s", ToStr(hr).c_str());
          return m_Overlay.resourceId;
        }
      }

      float clearColour[] = {0.0f, 1.0f, 0.0f, 0.0f};
      m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

      float overlayConsts[] = {0.0f, 1.0f, 0.0f, 1.0f};
      ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(overlayConsts, sizeof(overlayConsts));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->RSSetState(rs);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      SAFE_RELEASE(os);
      SAFE_RELEASE(rs);
    }
    else
    {
      RDCERR("Unhandled overlay case!");
    }
  }

  SAFE_RELEASE(rtv);

  SAFE_RELEASE(renderDepth);
  SAFE_RELEASE(preDrawDepth);

  return m_Overlay.resourceId;
}
