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

#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_resources.h"

struct Tex2DMSToArrayStateTracker
{
  Tex2DMSToArrayStateTracker(WrappedID3D11DeviceContext *wrappedContext)
  {
    m_WrappedContext = wrappedContext;
    D3D11RenderState *rs = wrappedContext->GetCurrentPipelineState();

    // first copy the properties. We don't need to keep refs as the objects won't be deleted by
    // being unbound and we won't do anything with them
    Layout = rs->IA.Layout;
    memcpy(&VS, &rs->VS, sizeof(VS));
    memcpy(&PS, &rs->PS, sizeof(PS));

    memcpy(CSUAVs, rs->CSUAVs, sizeof(CSUAVs));

    memcpy(&RS, &rs->RS, sizeof(RS));
    memcpy(&OM, &rs->OM, sizeof(OM));

    RDCCOMPILE_ASSERT(sizeof(VS) == sizeof(rs->VS), "Struct sizes have changed, ensure full copy");
    RDCCOMPILE_ASSERT(sizeof(RS) == sizeof(rs->RS), "Struct sizes have changed, ensure full copy");
    RDCCOMPILE_ASSERT(sizeof(OM) == sizeof(rs->OM), "Struct sizes have changed, ensure full copy");

    // now unwrap everything in place.
    Layout = UNWRAP(WrappedID3D11InputLayout, Layout);
    VS.Shader = UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, VS.Shader);
    PS.Shader = UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, PS.Shader);

    // only need to save/restore constant buffer 0
    PS.ConstantBuffers[0] = UNWRAP(WrappedID3D11Buffer, PS.ConstantBuffers[0]);

    // same for the first 16 SRVs
    for(int i = 0; i < 16; i++)
      PS.SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, PS.SRVs[i]);

    for(int i = 0; i < D3D11_SHADER_MAX_INTERFACES; i++)
    {
      VS.Instances[i] = UNWRAP(WrappedID3D11ClassInstance, VS.Instances[i]);
      PS.Instances[i] = UNWRAP(WrappedID3D11ClassInstance, PS.Instances[i]);
    }

    for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
      CSUAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, CSUAVs[i]);

    RS.State = UNWRAP(WrappedID3D11RasterizerState2, RS.State);
    OM.DepthStencilState = UNWRAP(WrappedID3D11DepthStencilState, OM.DepthStencilState);
    OM.BlendState = UNWRAP(WrappedID3D11BlendState1, OM.BlendState);
    OM.DepthView = UNWRAP(WrappedID3D11DepthStencilView, OM.DepthView);

    for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      OM.RenderTargets[i] = UNWRAP(WrappedID3D11RenderTargetView1, OM.RenderTargets[i]);

    for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
      OM.UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, OM.UAVs[i]);
  }
  ~Tex2DMSToArrayStateTracker()
  {
    ID3D11DeviceContext *context = m_WrappedContext->GetReal();
    ID3D11DeviceContext1 *context1 = m_WrappedContext->GetReal1();

    context->IASetInputLayout(Layout);
    context->VSSetShader((ID3D11VertexShader *)VS.Shader, VS.Instances, VS.NumInstances);

    context->PSSetShaderResources(0, 16, PS.SRVs);
    context->PSSetShader((ID3D11PixelShader *)PS.Shader, PS.Instances, PS.NumInstances);

    if(m_WrappedContext->IsFL11_1())
      context1->PSSetConstantBuffers1(0, 1, PS.ConstantBuffers, PS.CBOffsets, PS.CBCounts);
    else
      context->PSSetConstantBuffers(0, 1, PS.ConstantBuffers);

    UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1,
                                                   (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};

    if(m_WrappedContext->IsFL11_1())
      context->CSSetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, CSUAVs, UAV_keepcounts);
    else
      context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, CSUAVs, UAV_keepcounts);

    context->RSSetState(RS.State);
    context->RSSetViewports(RS.NumViews, RS.Viewports);

    context->OMSetBlendState(OM.BlendState, OM.BlendFactor, OM.SampleMask);
    context->OMSetDepthStencilState(OM.DepthStencilState, OM.StencRef);

    if(m_WrappedContext->IsFL11_1())
      context->OMSetRenderTargetsAndUnorderedAccessViews(
          OM.UAVStartSlot, OM.RenderTargets, OM.DepthView, OM.UAVStartSlot,
          D3D11_1_UAV_SLOT_COUNT - OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
    else
      context->OMSetRenderTargetsAndUnorderedAccessViews(
          OM.UAVStartSlot, OM.RenderTargets, OM.DepthView, OM.UAVStartSlot,
          D3D11_PS_CS_UAV_REGISTER_COUNT - OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
  }

  WrappedID3D11DeviceContext *m_WrappedContext;

  ID3D11InputLayout *Layout;

  struct shader
  {
    ID3D11DeviceChild *Shader;
    ID3D11Buffer *ConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    UINT CBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    UINT CBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11SamplerState *Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11ClassInstance *Instances[D3D11_SHADER_MAX_INTERFACES];
    UINT NumInstances;

    bool Used_CB(uint32_t slot) const;
    bool Used_SRV(uint32_t slot) const;
    bool Used_UAV(uint32_t slot) const;
  } VS, PS;

  ID3D11UnorderedAccessView *CSUAVs[D3D11_1_UAV_SLOT_COUNT];

  struct rasterizer
  {
    UINT NumViews, NumScissors;
    D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    ID3D11RasterizerState *State;
  } RS;

  struct outmerger
  {
    ID3D11DepthStencilState *DepthStencilState;
    UINT StencRef;

    ID3D11BlendState *BlendState;
    FLOAT BlendFactor[4];
    UINT SampleMask;

    ID3D11DepthStencilView *DepthView;

    ID3D11RenderTargetView *RenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

    UINT UAVStartSlot;
    ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT];
  } OM;
};

void D3D11DebugManager::CopyArrayToTex2DMS(ID3D11Texture2D *destMS, ID3D11Texture2D *srcArray,
                                           UINT selectedSlice)
{
  if(!CopyArrayToMSPS)
  {
    RDCWARN("Can't copy array to MSAA texture, contents will be undefined.");
    return;
  }

  bool singleSliceMode = (selectedSlice != ~0U);

  D3D11MarkerRegion copy("CopyArrayToTex2DMS");

  // unlike CopyTex2DMSToArray we can use the wrapped context here, but for consistency
  // we accept unwrapped parameters.

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  // copy to textures with right bind flags for operation
  D3D11_TEXTURE2D_DESC descArr;
  srcArray->GetDesc(&descArr);

  D3D11_TEXTURE2D_DESC descMS;
  destMS->GetDesc(&descMS);

  UINT sampleMask = ~0U;

  if(singleSliceMode)
  {
    sampleMask = 1U << (selectedSlice % descMS.SampleDesc.Count);
    selectedSlice /= descMS.SampleDesc.Count;
  }

  bool depthFormat = IsDepthFormat(descMS.Format);
  bool intFormat = IsUIntFormat(descMS.Format) || IsIntFormat(descMS.Format);

  ID3D11Texture2D *rtvResource = NULL;
  ID3D11Texture2D *srvResource = NULL;

  D3D11_TEXTURE2D_DESC rtvResDesc = descMS;
  D3D11_TEXTURE2D_DESC srvResDesc = descArr;

  rtvResDesc.BindFlags = depthFormat ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
  srvResDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  if(depthFormat)
  {
    rtvResDesc.Format = GetTypelessFormat(rtvResDesc.Format);
    srvResDesc.Format = GetTypelessFormat(srvResDesc.Format);
  }

  rtvResDesc.Usage = D3D11_USAGE_DEFAULT;
  srvResDesc.Usage = D3D11_USAGE_DEFAULT;

  rtvResDesc.CPUAccessFlags = 0;
  srvResDesc.CPUAccessFlags = 0;

  HRESULT hr = S_OK;

  hr = m_pDevice->CreateTexture2D(&rtvResDesc, NULL, &rtvResource);
  if(FAILED(hr))
  {
    RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
    return;
  }

  hr = m_pDevice->CreateTexture2D(&srvResDesc, NULL, &srvResource);
  if(FAILED(hr))
  {
    RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
    return;
  }

  // if we're doing a partial update, need to preserve what was in the destination texture
  if(singleSliceMode)
    m_pImmediateContext->GetReal()->CopyResource(UNWRAP(WrappedID3D11Texture2D1, rtvResource),
                                                 destMS);

  m_pImmediateContext->GetReal()->CopyResource(UNWRAP(WrappedID3D11Texture2D1, srvResource),
                                               srcArray);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {NULL};
  const UINT numUAVs =
      m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  UINT uavCounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&uavCounts[0], 0xff, sizeof(uavCounts));

  m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, uavs, uavCounts);

  m_pImmediateContext->VSSetShader(MSArrayCopyVS, NULL, 0);

  if(depthFormat)
    m_pImmediateContext->PSSetShader(DepthCopyArrayToMSPS, NULL, 0);
  else if(intFormat)
    m_pImmediateContext->PSSetShader(CopyArrayToMSPS, NULL, 0);
  else
    m_pImmediateContext->PSSetShader(FloatCopyArrayToMSPS, NULL, 0);

  m_pImmediateContext->HSSetShader(NULL, NULL, 0);
  m_pImmediateContext->DSSetShader(NULL, NULL, 0);
  m_pImmediateContext->GSSetShader(NULL, NULL, 0);

  D3D11_VIEWPORT view = {0.0f, 0.0f, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f};

  m_pImmediateContext->RSSetState(NULL);
  m_pImmediateContext->RSSetViewports(1, &view);

  m_pImmediateContext->IASetInputLayout(NULL);
  m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  float blendFactor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  m_pImmediateContext->OMSetBlendState(NULL, blendFactor, sampleMask);

  {
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    if(depthFormat)
      dsDesc.StencilEnable = FALSE;
    else
      dsDesc.StencilEnable = TRUE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);
    m_pImmediateContext->OMSetDepthStencilState(dsState, 0);
    SAFE_RELEASE(dsState);
  }

  ID3D11DepthStencilView *dsvMS = NULL;
  ID3D11RenderTargetView *rtvMS = NULL;
  ID3D11ShaderResourceView *srvArray = NULL;

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
  rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
  rtvDesc.Format = depthFormat ? GetUIntTypedFormat(descMS.Format)
                               : GetTypedFormat(descMS.Format, CompType::UInt);
  rtvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
  rtvDesc.Texture2DMSArray.FirstArraySlice = 0;

  D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
  dsvDesc.Flags = 0;
  dsvDesc.Format = GetDepthTypedFormat(descMS.Format);
  dsvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
  dsvDesc.Texture2DMSArray.FirstArraySlice = 0;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc.Format = depthFormat ? GetUIntTypedFormat(descArr.Format)
                               : GetTypedFormat(descArr.Format, CompType::UInt);
  srvDesc.Texture2DArray.ArraySize = descArr.ArraySize;
  srvDesc.Texture2DArray.FirstArraySlice = 0;
  srvDesc.Texture2DArray.MipLevels = descArr.MipLevels;
  srvDesc.Texture2DArray.MostDetailedMip = 0;

  bool stencil = false;
  DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

  if(depthFormat)
  {
    switch(descArr.Format)
    {
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_FLOAT; break;
    }
  }

  hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvArray);
  if(FAILED(hr))
  {
    RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
    return;
  }

  ID3D11ShaderResourceView *srvs[10] = {NULL};
  srvs[0] = srvArray;

  m_pImmediateContext->PSSetShaderResources(1, 10, srvs);

  // loop over every array slice in MS texture
  for(UINT slice = 0; slice < descMS.ArraySize; slice++)
  {
    if(singleSliceMode)
      slice = selectedSlice;

    uint32_t cdata[4] = {descMS.SampleDesc.Count, 1000, 0, slice};

    ID3D11Buffer *cbuf = MakeCBuffer(cdata, sizeof(cdata));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

    rtvDesc.Texture2DMSArray.FirstArraySlice = slice;
    rtvDesc.Texture2DMSArray.ArraySize = 1;
    dsvDesc.Texture2DMSArray.FirstArraySlice = slice;
    dsvDesc.Texture2DMSArray.ArraySize = 1;

    if(depthFormat)
      hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvMS);
    else
      hr = m_pDevice->CreateRenderTargetView(rtvResource, &rtvDesc, &rtvMS);
    if(FAILED(hr))
    {
      SAFE_RELEASE(srvArray);
      RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
      return;
    }

    if(depthFormat)
      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvMS, 0, 0, NULL,
                                                                     NULL);
    else
      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtvMS, NULL, 0, 0, NULL,
                                                                     NULL);

    m_pImmediateContext->Draw(3, 0);

    SAFE_RELEASE(rtvMS);
    SAFE_RELEASE(dsvMS);

    if(singleSliceMode)
      break;
  }

  SAFE_RELEASE(srvArray);

  if(stencil)
  {
    srvDesc.Format = stencilFormat;

    hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvArray);
    if(FAILED(hr))
    {
      RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
      return;
    }

    m_pImmediateContext->PSSetShaderResources(11, 1, &srvArray);

    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.StencilEnable = TRUE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
    dsvDesc.Texture2DArray.ArraySize = 1;

    m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);

    // loop over every array slice in MS texture
    for(UINT slice = 0; slice < descMS.ArraySize; slice++)
    {
      if(singleSliceMode)
        slice = selectedSlice;

      dsvDesc.Texture2DMSArray.FirstArraySlice = slice;

      hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvMS);
      if(FAILED(hr))
      {
        SAFE_RELEASE(srvArray);
        SAFE_RELEASE(dsState);
        RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
        return;
      }

      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvMS, 0, 0, NULL,
                                                                     NULL);

      // loop over every stencil value (zzzzzz, no shader stencil read/write)
      for(UINT stencilval = 0; stencilval < 256; stencilval++)
      {
        uint32_t cdata[4] = {descMS.SampleDesc.Count, stencilval, 0, slice};

        ID3D11Buffer *cbuf = MakeCBuffer(cdata, sizeof(cdata));

        m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

        m_pImmediateContext->OMSetDepthStencilState(dsState, stencilval);

        m_pImmediateContext->Draw(3, 0);
      }

      SAFE_RELEASE(dsvMS);

      m_pImmediateContext->Flush();

      if(singleSliceMode)
        break;
    }

    SAFE_RELEASE(srvArray);
    SAFE_RELEASE(dsState);
  }

  m_pImmediateContext->GetReal()->CopyResource(destMS, UNWRAP(WrappedID3D11Texture2D1, rtvResource));

  SAFE_RELEASE(rtvResource);
  SAFE_RELEASE(srvResource);
}

void D3D11DebugManager::CopyTex2DMSToArray(ID3D11Texture2D *destArray, ID3D11Texture2D *srcMS)
{
  if(!CopyMSToArrayPS)
  {
    RDCWARN("Can't copy array to MSAA texture, contents will be undefined.");
    return;
  }

  // we have to use exclusively the unwrapped context here as this might be happening during
  // capture and we don't want to serialise any of this work, and the parameters might not exist
  // as wrapped objects for that reason

  // use the wrapped context's state tracked to avoid needing our own tracking, just restore it to
  // the unwrapped context
  Tex2DMSToArrayStateTracker tracker(m_pImmediateContext);

  D3D11MarkerRegion copy("CopyTex2DMSToArray");

  ID3D11Device *dev = m_pDevice->GetReal();
  ID3D11DeviceContext *ctx = m_pImmediateContext->GetReal();

  // copy to textures with right bind flags for operation
  D3D11_TEXTURE2D_DESC descMS;
  srcMS->GetDesc(&descMS);

  D3D11_TEXTURE2D_DESC descArr;
  destArray->GetDesc(&descArr);

  ID3D11Texture2D *rtvResource = NULL;
  ID3D11Texture2D *srvResource = NULL;

  D3D11_TEXTURE2D_DESC rtvResDesc = descArr;
  D3D11_TEXTURE2D_DESC srvResDesc = descMS;

  bool depthFormat = IsDepthFormat(descMS.Format);
  bool intFormat = IsUIntFormat(descMS.Format) || IsIntFormat(descMS.Format);

  rtvResDesc.BindFlags = depthFormat ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
  srvResDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  if(depthFormat)
  {
    rtvResDesc.Format = GetTypelessFormat(rtvResDesc.Format);
    srvResDesc.Format = GetTypelessFormat(srvResDesc.Format);
  }

  rtvResDesc.Usage = D3D11_USAGE_DEFAULT;
  srvResDesc.Usage = D3D11_USAGE_DEFAULT;

  rtvResDesc.CPUAccessFlags = 0;
  srvResDesc.CPUAccessFlags = 0;

  HRESULT hr = S_OK;

  hr = dev->CreateTexture2D(&rtvResDesc, NULL, &rtvResource);
  if(FAILED(hr))
  {
    RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
    return;
  }

  hr = dev->CreateTexture2D(&srvResDesc, NULL, &srvResource);
  if(FAILED(hr))
  {
    RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
    return;
  }

  ctx->CopyResource(srvResource, srcMS);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {NULL};
  UINT uavCounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&uavCounts[0], 0xff, sizeof(uavCounts));
  const UINT numUAVs =
      m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;

  ctx->CSSetUnorderedAccessViews(0, numUAVs, uavs, uavCounts);

  ctx->VSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, MSArrayCopyVS), NULL, 0);
  if(depthFormat)
    ctx->PSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, DepthCopyMSToArrayPS), NULL, 0);
  else if(intFormat)
    ctx->PSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, CopyMSToArrayPS), NULL, 0);
  else
    ctx->PSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, FloatCopyMSToArrayPS), NULL, 0);

  D3D11_VIEWPORT view = {0.0f, 0.0f, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f};

  ctx->RSSetState(NULL);
  ctx->RSSetViewports(1, &view);

  ctx->IASetInputLayout(NULL);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  float blendFactor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  ctx->OMSetBlendState(NULL, blendFactor, ~0U);

  {
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    if(depthFormat)
      dsDesc.StencilEnable = FALSE;
    else
      dsDesc.StencilEnable = TRUE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    dev->CreateDepthStencilState(&dsDesc, &dsState);
    ctx->OMSetDepthStencilState(dsState, 0);
    SAFE_RELEASE(dsState);
  }

  ID3D11RenderTargetView *rtvArray = NULL;
  ID3D11DepthStencilView *dsvArray = NULL;
  ID3D11ShaderResourceView *srvMS = NULL;

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
  rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
  rtvDesc.Format = depthFormat ? GetUIntTypedFormat(descArr.Format)
                               : GetTypedFormat(descArr.Format, CompType::UInt);
  rtvDesc.Texture2DArray.FirstArraySlice = 0;
  rtvDesc.Texture2DArray.ArraySize = 1;
  rtvDesc.Texture2DArray.MipSlice = 0;

  D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
  dsvDesc.Format = GetDepthTypedFormat(descArr.Format);
  dsvDesc.Flags = 0;
  dsvDesc.Texture2DArray.FirstArraySlice = 0;
  dsvDesc.Texture2DArray.ArraySize = 1;
  dsvDesc.Texture2DArray.MipSlice = 0;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
  srvDesc.Format = depthFormat ? GetUIntTypedFormat(descMS.Format)
                               : GetTypedFormat(descMS.Format, CompType::UInt);
  srvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
  srvDesc.Texture2DMSArray.FirstArraySlice = 0;

  bool stencil = false;
  DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

  if(depthFormat)
  {
    switch(descMS.Format)
    {
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_FLOAT; break;
    }
  }

  hr = dev->CreateShaderResourceView(srvResource, &srvDesc, &srvMS);
  if(FAILED(hr))
  {
    RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
    return;
  }

  ID3D11ShaderResourceView *srvs[16] = {NULL};

  int srvIndex = 0;

  for(int i = 0; i < 8; i++)
    if(descMS.SampleDesc.Count == UINT(1 << i))
      srvIndex = i;

  srvs[srvIndex] = srvMS;

  ctx->PSSetShaderResources(0, 16, srvs);

  // loop over every array slice in MS texture
  for(UINT slice = 0; slice < descMS.ArraySize; slice++)
  {
    // loop over every multi sample
    for(UINT sample = 0; sample < descMS.SampleDesc.Count; sample++)
    {
      uint32_t cdata[4] = {descMS.SampleDesc.Count, 1000, sample, slice};

      ID3D11Buffer *cbuf = UNWRAP(WrappedID3D11Buffer, MakeCBuffer(cdata, sizeof(cdata)));

      ctx->PSSetConstantBuffers(0, 1, &cbuf);

      rtvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;
      dsvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;

      if(depthFormat)
        hr = dev->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvArray);
      else
        hr = dev->CreateRenderTargetView(rtvResource, &rtvDesc, &rtvArray);

      if(FAILED(hr))
      {
        SAFE_RELEASE(rtvArray);
        SAFE_RELEASE(dsvArray);
        RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
        return;
      }

      if(depthFormat)
        ctx->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvArray, 0, 0, NULL, NULL);
      else
        ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtvArray, NULL, 0, 0, NULL, NULL);

      ctx->Draw(3, 0);

      SAFE_RELEASE(rtvArray);
      SAFE_RELEASE(dsvArray);

      ctx->Flush();
    }
  }

  SAFE_RELEASE(srvMS);

  if(stencil)
  {
    srvDesc.Format = stencilFormat;

    hr = dev->CreateShaderResourceView(srvResource, &srvDesc, &srvMS);
    if(FAILED(hr))
    {
      RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
      return;
    }

    ctx->PSSetShaderResources(10 + srvIndex, 1, &srvMS);

    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.StencilEnable = TRUE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
    dsvDesc.Texture2DArray.ArraySize = 1;

    dev->CreateDepthStencilState(&dsDesc, &dsState);

    // loop over every array slice in MS texture
    for(UINT slice = 0; slice < descMS.ArraySize; slice++)
    {
      // loop over every multi sample
      for(UINT sample = 0; sample < descMS.SampleDesc.Count; sample++)
      {
        dsvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;

        hr = dev->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvArray);
        if(FAILED(hr))
        {
          SAFE_RELEASE(dsState);
          SAFE_RELEASE(srvMS);
          RDCERR("0xHRESULT: %s", ToStr(hr).c_str());
          return;
        }

        ctx->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvArray, 0, 0, NULL, NULL);

        // loop over every stencil value (zzzzzz, no shader stencil read/write)
        for(UINT stencilval = 0; stencilval < 256; stencilval++)
        {
          uint32_t cdata[4] = {descMS.SampleDesc.Count, stencilval, sample, slice};

          ID3D11Buffer *cbuf = UNWRAP(WrappedID3D11Buffer, MakeCBuffer(cdata, sizeof(cdata)));

          ctx->PSSetConstantBuffers(0, 1, &cbuf);

          ctx->OMSetDepthStencilState(dsState, stencilval);

          ctx->Draw(3, 0);
        }

        ctx->Flush();

        SAFE_RELEASE(dsvArray);
      }
    }

    SAFE_RELEASE(dsState);
    SAFE_RELEASE(srvMS);
  }

  ctx->CopyResource(destArray, rtvResource);

  SAFE_RELEASE(rtvResource);
  SAFE_RELEASE(srvResource);
}
