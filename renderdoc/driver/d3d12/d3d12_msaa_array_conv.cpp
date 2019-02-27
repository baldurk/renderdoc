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

#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"

void D3D12DebugManager::CopyTex2DMSToArray(ID3D12Resource *destArray, ID3D12Resource *srcMS)
{
  // this function operates during capture so we work on unwrapped objects

  D3D12_RESOURCE_DESC descMS = srcMS->GetDesc();
  D3D12_RESOURCE_DESC descArr = destArray->GetDesc();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
  srvDesc.Texture2DMSArray.FirstArraySlice = 0;
  srvDesc.Texture2DMSArray.ArraySize = descMS.DepthOrArraySize;
  srvDesc.Format = GetTypedFormat(descMS.Format, CompType::UInt);
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
  rtvDesc.Format = srvDesc.Format;
  rtvDesc.Texture2DArray.ArraySize = 1;
  rtvDesc.Texture2DArray.FirstArraySlice = 0;
  rtvDesc.Texture2DArray.MipSlice = 0;
  rtvDesc.Texture2DArray.PlaneSlice = 0;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
  dsvDesc.Format = srvDesc.Format;
  dsvDesc.Texture2DArray.ArraySize = 1;
  dsvDesc.Texture2DArray.FirstArraySlice = 0;
  dsvDesc.Texture2DArray.MipSlice = 0;

  bool isDepth = IsDepthFormat(rtvDesc.Format) ||
                 (descMS.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
  bool intFormat = IsUIntFormat(rtvDesc.Format) || IsIntFormat(rtvDesc.Format);

  bool stencil = false;
  DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

  if(isDepth)
  {
    switch(descMS.Format)
    {
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS:
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_TYPELESS:
        dsvDesc.Format = DXGI_FORMAT_D16_UNORM;
        srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
        break;
    }
  }

  for(CBVUAVSRVSlot slot : {MSAA_SRV2x, MSAA_SRV4x, MSAA_SRV8x, MSAA_SRV16x, MSAA_SRV32x})
  {
    D3D12_CPU_DESCRIPTOR_HANDLE srv = Unwrap(GetCPUHandle(slot));
    m_pDevice->GetReal()->CreateShaderResourceView(srcMS, &srvDesc, srv);
  }

  if(stencil)
  {
    srvDesc.Format = stencilFormat;

    for(CBVUAVSRVSlot slot : {STENCIL_MSAA_SRV2x, STENCIL_MSAA_SRV4x, STENCIL_MSAA_SRV8x,
                              STENCIL_MSAA_SRV16x, STENCIL_MSAA_SRV32x})
    {
      D3D12_CPU_DESCRIPTOR_HANDLE srv = Unwrap(GetCPUHandle(slot));
      m_pDevice->GetReal()->CreateShaderResourceView(srcMS, &srvDesc, srv);
    }
  }

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = Unwrap(GetCPUHandle(MSAA_RTV));
  D3D12_CPU_DESCRIPTOR_HANDLE dsv = Unwrap(GetCPUHandle(MSAA_DSV));

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

  pipeDesc.pRootSignature = Unwrap(m_ArrayMSAARootSig);
  pipeDesc.VS.BytecodeLength = m_FullscreenVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = m_FullscreenVS->GetBufferPointer();

  pipeDesc.PS.BytecodeLength = m_FloatMS2Array->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = m_FloatMS2Array->GetBufferPointer();
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = rtvDesc.Format;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

  if(isDepth)
  {
    pipeDesc.PS.BytecodeLength = m_DepthMS2Array->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = m_DepthMS2Array->GetBufferPointer();
    pipeDesc.NumRenderTargets = 0;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    pipeDesc.DSVFormat = dsvDesc.Format;
    pipeDesc.DepthStencilState.DepthEnable = TRUE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  }
  else if(intFormat)
  {
    pipeDesc.PS.BytecodeLength = m_IntMS2Array->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = m_IntMS2Array->GetBufferPointer();
  }

  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  ID3D12PipelineState *pso = NULL, *psoStencil = NULL;
  HRESULT hr = m_pDevice->GetReal()->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState), (void **)&pso);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create MSAA conversion pipeline! HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  ID3D12GraphicsCommandList *list = Unwrap(m_DebugList);

  list->Reset(Unwrap(m_DebugAlloc), NULL);

  D3D12_VIEWPORT viewport = {0, 0, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f};
  D3D12_RECT scissor = {0, 0, (LONG)descArr.Width, (LONG)descArr.Height};
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &scissor);
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  ID3D12DescriptorHeap *heap = Unwrap(cbvsrvuavHeap);
  list->SetDescriptorHeaps(1, &heap);
  list->SetPipelineState(pso);
  list->SetGraphicsRootSignature(Unwrap(m_ArrayMSAARootSig));
  list->SetGraphicsRootDescriptorTable(1, Unwrap(GetGPUHandle(MSAA_SRV2x)));
  if(stencil)
    list->SetGraphicsRootDescriptorTable(2, Unwrap(GetGPUHandle(STENCIL_MSAA_SRV2x)));

  // loop over every array slice in MS texture
  for(UINT slice = 0; slice < descMS.DepthOrArraySize; slice++)
  {
    // loop over every multi sample
    for(UINT sample = 0; sample < descMS.SampleDesc.Count; sample++)
    {
      uint32_t cdata[4] = {descMS.SampleDesc.Count, 1000, sample, slice};

      list->SetGraphicsRootConstantBufferView(0, UploadConstants(cdata, sizeof(cdata)));

      dsvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;
      rtvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;

      if(isDepth)
      {
        m_pDevice->GetReal()->CreateDepthStencilView(destArray, &dsvDesc, dsv);
        list->OMSetRenderTargets(0, NULL, FALSE, &dsv);
      }
      else
      {
        m_pDevice->GetReal()->CreateRenderTargetView(destArray, &rtvDesc, rtv);
        list->OMSetRenderTargets(1, &rtv, FALSE, NULL);
      }

      list->DrawInstanced(3, 1, 0, 0);
    }
  }

  if(stencil)
  {
    pipeDesc.DepthStencilState.DepthEnable = FALSE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeDesc.DepthStencilState.StencilEnable = TRUE;
    pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
    pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
    pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;
    pipeDesc.DepthStencilState.StencilReadMask = 0xff;
    pipeDesc.DepthStencilState.StencilWriteMask = 0xff;

    hr = m_pDevice->GetReal()->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                           (void **)&psoStencil);
    RDCASSERTEQUAL(hr, S_OK);

    list->SetPipelineState(psoStencil);

    dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    dsvDesc.Texture2DArray.ArraySize = 1;

    // loop over every array slice in MS texture
    for(UINT slice = 0; slice < descMS.DepthOrArraySize; slice++)
    {
      // loop over every multi sample
      for(UINT sample = 0; sample < descMS.SampleDesc.Count; sample++)
      {
        dsvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;
        m_pDevice->GetReal()->CreateDepthStencilView(destArray, &dsvDesc, dsv);
        list->OMSetRenderTargets(0, NULL, FALSE, &dsv);

        // loop over every stencil value (in theory we could use SV_StencilRef, but it's optional
        // and would mean a different shader)
        for(UINT stencilval = 0; stencilval < 256; stencilval++)
        {
          uint32_t cdata[4] = {descMS.SampleDesc.Count, stencilval, sample, slice};

          list->SetGraphicsRootConstantBufferView(0, UploadConstants(cdata, sizeof(cdata)));

          list->OMSetStencilRef(stencilval);

          list->DrawInstanced(3, 1, 0, 0);
        }
      }
    }
  }

  list->Close();

  ID3D12Fence *tmpFence = NULL;
  m_pDevice->GetReal()->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                    (void **)&tmpFence);

  ID3D12CommandList *l = list;
  m_pDevice->GetQueue()->GetReal()->ExecuteCommandLists(1, &l);
  m_pDevice->GPUSync(m_pDevice->GetQueue()->GetReal(), tmpFence);
  m_DebugAlloc->Reset();

  SAFE_RELEASE(tmpFence);
  SAFE_RELEASE(pso);
  SAFE_RELEASE(psoStencil);
}

void D3D12DebugManager::CopyArrayToTex2DMS(ID3D12Resource *destMS, ID3D12Resource *srcArray,
                                           UINT selectedSlice)
{
  bool singleSliceMode = (selectedSlice != ~0U);

  D3D12_RESOURCE_DESC descArr = srcArray->GetDesc();
  D3D12_RESOURCE_DESC descMS = destMS->GetDesc();

  UINT sampleMask = ~0U;

  if(singleSliceMode)
  {
    sampleMask = 1U << (selectedSlice % descMS.SampleDesc.Count);
    selectedSlice /= descMS.SampleDesc.Count;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc.Texture2DArray.MipLevels = 1;
  srvDesc.Texture2DArray.ArraySize = descArr.DepthOrArraySize;
  srvDesc.Format = GetTypedFormat(descMS.Format, CompType::UInt);
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
  rtvDesc.Format = srvDesc.Format;
  rtvDesc.Texture2DMSArray.ArraySize = 1;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
  dsvDesc.Format = srvDesc.Format;
  dsvDesc.Texture2DMSArray.ArraySize = 1;

  bool isDepth = IsDepthFormat(rtvDesc.Format) ||
                 (descArr.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
  bool intFormat = IsUIntFormat(rtvDesc.Format) || IsIntFormat(rtvDesc.Format);

  bool stencil = false;
  DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

  if(isDepth)
  {
    switch(descMS.Format)
    {
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS:
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_TYPELESS:
        dsvDesc.Format = DXGI_FORMAT_D16_UNORM;
        srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
        break;
    }
  }

  for(CBVUAVSRVSlot slot : {MSAA_SRV2x, MSAA_SRV4x, MSAA_SRV8x, MSAA_SRV16x, MSAA_SRV32x})
  {
    D3D12_CPU_DESCRIPTOR_HANDLE srv = GetCPUHandle(slot);
    m_pDevice->CreateShaderResourceView(srcArray, &srvDesc, srv);
  }

  if(stencil)
  {
    srvDesc.Format = stencilFormat;

    {
      D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
      formatInfo.Format = srvDesc.Format;
      m_pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

      UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

      if(planes > 1)
        srvDesc.Texture2DArray.PlaneSlice = 1;
    }

    for(CBVUAVSRVSlot slot : {STENCIL_MSAA_SRV2x, STENCIL_MSAA_SRV4x, STENCIL_MSAA_SRV8x,
                              STENCIL_MSAA_SRV16x, STENCIL_MSAA_SRV32x})
    {
      D3D12_CPU_DESCRIPTOR_HANDLE srv = GetCPUHandle(slot);
      m_pDevice->CreateShaderResourceView(srcArray, &srvDesc, srv);
    }
  }

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCPUHandle(MSAA_RTV);
  D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetCPUHandle(MSAA_DSV);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

  pipeDesc.pRootSignature = m_ArrayMSAARootSig;
  pipeDesc.VS.BytecodeLength = m_FullscreenVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = m_FullscreenVS->GetBufferPointer();

  pipeDesc.PS.BytecodeLength = m_FloatArray2MS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = m_FloatArray2MS->GetBufferPointer();
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = rtvDesc.Format;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

  if(isDepth)
  {
    pipeDesc.PS.BytecodeLength = m_DepthArray2MS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = m_DepthArray2MS->GetBufferPointer();
    pipeDesc.NumRenderTargets = 0;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    pipeDesc.DSVFormat = dsvDesc.Format;
    pipeDesc.DepthStencilState.DepthEnable = TRUE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  }
  else if(intFormat)
  {
    pipeDesc.PS.BytecodeLength = m_IntArray2MS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = m_IntArray2MS->GetBufferPointer();
  }

  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = sampleMask;
  pipeDesc.SampleDesc = descMS.SampleDesc;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  ID3D12PipelineState *pso = NULL, *psoStencil = NULL;
  HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&pso);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create MSAA conversion pipeline! HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  ID3D12GraphicsCommandList *list = m_DebugList;

  list->Reset(m_DebugAlloc, NULL);

  D3D12_VIEWPORT viewport = {0, 0, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f};
  D3D12_RECT scissor = {0, 0, (LONG)descArr.Width, (LONG)descArr.Height};
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &scissor);
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  list->SetDescriptorHeaps(1, &cbvsrvuavHeap);
  list->SetPipelineState(pso);
  list->SetGraphicsRootSignature(m_ArrayMSAARootSig);
  list->SetGraphicsRootDescriptorTable(1, GetGPUHandle(MSAA_SRV2x));
  if(stencil)
    list->SetGraphicsRootDescriptorTable(2, GetGPUHandle(STENCIL_MSAA_SRV2x));

  // loop over every array slice in MS texture
  for(UINT slice = 0; slice < descMS.DepthOrArraySize; slice++)
  {
    if(singleSliceMode)
      slice = selectedSlice;

    uint32_t cdata[4] = {descMS.SampleDesc.Count, 1000, 0, slice};

    list->SetGraphicsRootConstantBufferView(0, UploadConstants(cdata, sizeof(cdata)));

    rtvDesc.Texture2DMSArray.FirstArraySlice = slice;
    dsvDesc.Texture2DMSArray.FirstArraySlice = slice;

    if(isDepth)
    {
      m_pDevice->CreateDepthStencilView(destMS, &dsvDesc, dsv);
      list->OMSetRenderTargets(0, NULL, FALSE, &dsv);
    }
    else
    {
      m_pDevice->CreateRenderTargetView(destMS, &rtvDesc, rtv);
      list->OMSetRenderTargets(1, &rtv, FALSE, NULL);
    }

    list->DrawInstanced(3, 1, 0, 0);

    if(singleSliceMode)
      break;
  }

  if(stencil)
  {
    pipeDesc.DepthStencilState.DepthEnable = FALSE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeDesc.DepthStencilState.StencilEnable = TRUE;
    pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
    pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
    pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;
    pipeDesc.DepthStencilState.StencilReadMask = 0xff;
    pipeDesc.DepthStencilState.StencilWriteMask = 0xff;

    hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                (void **)&psoStencil);
    RDCASSERTEQUAL(hr, S_OK);

    dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    dsvDesc.Texture2DMSArray.ArraySize = 1;
    m_pDevice->CreateDepthStencilView(destMS, &dsvDesc, dsv);
    list->OMSetRenderTargets(0, NULL, FALSE, &dsv);
    list->SetPipelineState(psoStencil);

    // loop over every array slice in MS texture
    for(UINT slice = 0; slice < descMS.DepthOrArraySize; slice++)
    {
      if(singleSliceMode)
        slice = selectedSlice;

      dsvDesc.Texture2DArray.FirstArraySlice = slice;

      // loop over every stencil value (in theory we could use SV_StencilRef, but it's optional
      // and would mean a different shader)
      for(UINT stencilval = 0; stencilval < 256; stencilval++)
      {
        uint32_t cdata[4] = {descMS.SampleDesc.Count, stencilval, 0, slice};

        list->SetGraphicsRootConstantBufferView(0, UploadConstants(cdata, sizeof(cdata)));

        list->OMSetStencilRef(stencilval);

        list->DrawInstanced(3, 1, 0, 0);
      }
    }
  }

  list->Close();

  ID3D12CommandList *l = m_DebugList;
  m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
  m_pDevice->GPUSync();
  m_DebugAlloc->Reset();

  SAFE_RELEASE(pso);
  SAFE_RELEASE(psoStencil);
}
