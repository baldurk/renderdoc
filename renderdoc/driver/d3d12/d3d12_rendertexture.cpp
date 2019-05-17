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

#include "driver/dxgi/dxgi_common.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"

#include "data/hlsl/hlsl_cbuffers.h"

void D3D12DebugManager::PrepareTextureSampling(ID3D12Resource *resource, CompType typeHint,
                                               int &resType,
                                               std::vector<D3D12_RESOURCE_BARRIER> &barriers)
{
  int srvOffset = 0;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = GetTypedFormat(resourceDesc.Format, typeHint);
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    srvOffset = RESTYPE_TEX3D;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = ~0U;
  }
  else if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
  {
    if(resourceDesc.SampleDesc.Count > 1)
    {
      srvOffset = RESTYPE_TEX2D_MS;
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
      srvDesc.Texture2DMSArray.ArraySize = ~0U;

      if(IsDepthFormat(resourceDesc.Format))
        srvOffset = RESTYPE_DEPTH_MS;
    }
    else
    {
      srvOffset = RESTYPE_TEX2D;
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2D.MipLevels = ~0U;
      srvDesc.Texture2DArray.ArraySize = ~0U;

      if(IsDepthFormat(resourceDesc.Format))
        srvOffset = RESTYPE_DEPTH;
    }
  }
  else if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    srvOffset = RESTYPE_TEX1D;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    srvDesc.Texture1DArray.MipLevels = ~0U;
    srvDesc.Texture1DArray.ArraySize = ~0U;
  }

  resType = srvOffset;

  // if it's a depth and stencil image, increment (as the restype for
  // depth/stencil is one higher than that for depth only).
  if(IsDepthAndStencilFormat(resourceDesc.Format))
    resType++;

  if(IsUIntFormat(resourceDesc.Format))
    srvOffset += 10;
  if(IsIntFormat(resourceDesc.Format))
    srvOffset += 20;

  D3D12_RESOURCE_STATES realResourceState =
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  bool copy = false;

  D3D12_SHADER_RESOURCE_VIEW_DESC altSRVDesc = {};

  // for non-typeless depth formats, we need to copy to a typeless resource for read
  if(IsDepthFormat(resourceDesc.Format) &&
     GetTypelessFormat(resourceDesc.Format) != resourceDesc.Format)
  {
    realResourceState = D3D12_RESOURCE_STATE_COPY_SOURCE;
    copy = true;

    switch(GetTypelessFormat(srvDesc.Format))
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        altSRVDesc = srvDesc;
        altSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      case DXGI_FORMAT_R24G8_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        altSRVDesc = srvDesc;
        altSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
      case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
      case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_UNORM; break;
      default:
        RDCERR("Unexpected typeless format %d from depth format %d",
               GetTypelessFormat(srvDesc.Format), srvDesc.Format);
        break;
    }
  }

  if(IsYUVFormat(resourceDesc.Format))
  {
    altSRVDesc = srvDesc;
    srvDesc.Format = GetYUVViewPlane0Format(resourceDesc.Format);
    altSRVDesc.Format = GetYUVViewPlane1Format(resourceDesc.Format);

    // assume YUV textures are 2D or 2D arrays
    RDCASSERT(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

    // the second SRV, if used, is always for the second plane
    altSRVDesc.Texture2DArray.PlaneSlice = 1;
  }

  // even for non-copies, we need to make two SRVs to sample stencil as well
  if(IsDepthAndStencilFormat(resourceDesc.Format) && altSRVDesc.Format == DXGI_FORMAT_UNKNOWN)
  {
    switch(GetTypelessFormat(srvDesc.Format))
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        altSRVDesc = srvDesc;
        altSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      case DXGI_FORMAT_R24G8_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        altSRVDesc = srvDesc;
        altSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
    }
  }

  if(altSRVDesc.Format != DXGI_FORMAT_UNKNOWN && !IsYUVFormat(resourceDesc.Format))
  {
    D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
    formatInfo.Format = srvDesc.Format;
    m_pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

    if(formatInfo.PlaneCount > 1 && altSRVDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      altSRVDesc.Texture2DArray.PlaneSlice = 1;
  }

  // transition resource to D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  const std::vector<D3D12_RESOURCE_STATES> &states =
      m_pDevice->GetSubresourceStates(GetResID(resource));

  barriers.reserve(states.size());
  for(size_t i = 0; i < states.size(); i++)
  {
    D3D12_RESOURCE_BARRIER b;

    // skip unneeded barriers
    if((states[i] & realResourceState) == realResourceState)
      continue;

    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = resource;
    b.Transition.Subresource = (UINT)i;
    b.Transition.StateBefore = states[i];
    b.Transition.StateAfter = realResourceState;

    barriers.push_back(b);
  }

  if(copy)
  {
    D3D12_RESOURCE_DESC resDesc = resource->GetDesc();

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Alignment = 0;
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Format = GetTypelessFormat(resDesc.Format);
    texDesc.Width = resDesc.Width;
    texDesc.Height = resDesc.Height;
    texDesc.DepthOrArraySize = resDesc.DepthOrArraySize;
    texDesc.MipLevels = resDesc.MipLevels;
    texDesc.SampleDesc.Count = resDesc.SampleDesc.Count;
    texDesc.SampleDesc.Quality = 0;

    if(texDesc.SampleDesc.Count > 1)
      texDesc.Flags |= IsDepthFormat(texDesc.Format) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                                                     : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // check if the existing resource is similar enough (same typeless format and dimension)
    if(m_TexResource)
    {
      D3D12_RESOURCE_DESC oldDesc = m_TexResource->GetDesc();

      if(oldDesc.Width != texDesc.Width || oldDesc.Height != texDesc.Height ||
         oldDesc.DepthOrArraySize != texDesc.DepthOrArraySize || oldDesc.Format != texDesc.Format ||
         oldDesc.MipLevels != texDesc.MipLevels ||
         oldDesc.SampleDesc.Count != texDesc.SampleDesc.Count)
      {
        SAFE_RELEASE(m_TexResource);
      }
    }

    // create resource if we need it
    if(!m_TexResource)
    {
      HRESULT hr = m_pDevice->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
          NULL, __uuidof(ID3D12Resource), (void **)&m_TexResource);
      RDCASSERTEQUAL(hr, S_OK);

      m_TexResource->SetName(L"m_TexResource");
    }

    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    // prepare real resource for copying
    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    D3D12_RESOURCE_BARRIER texResourceBarrier;
    D3D12_RESOURCE_BARRIER &b = texResourceBarrier;

    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = m_TexResource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    // prepare tex resource for copying
    list->ResourceBarrier(1, &texResourceBarrier);

    list->CopyResource(m_TexResource, resource);

    // tex resource back to readable
    std::swap(texResourceBarrier.Transition.StateBefore, texResourceBarrier.Transition.StateAfter);
    list->ResourceBarrier(1, &texResourceBarrier);

    // real resource back to itself
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    // don't do any barriers outside in the source function
    barriers.clear();

    list->Close();

    resource = m_TexResource;
  }

  // empty all the other SRVs just to mute debug warnings
  D3D12_CPU_DESCRIPTOR_HANDLE srv = GetCPUHandle(FIRST_TEXDISPLAY_SRV);

  D3D12_SHADER_RESOURCE_VIEW_DESC emptyDesc = {};
  emptyDesc.Format = DXGI_FORMAT_R8_UNORM;
  emptyDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  emptyDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  emptyDesc.Texture2D.MipLevels = 1;

  for(size_t i = 0; i < 32; i++)
  {
    m_pDevice->CreateShaderResourceView(NULL, &emptyDesc, srv);
    srv.ptr += sizeof(D3D12Descriptor);
  }

  srv = GetCPUHandle(FIRST_TEXDISPLAY_SRV);
  srv.ptr += srvOffset * sizeof(D3D12Descriptor);

  m_pDevice->CreateShaderResourceView(resource, &srvDesc, srv);
  if(altSRVDesc.Format != DXGI_FORMAT_UNKNOWN)
  {
    if(IsYUVFormat(resourceDesc.Format))
    {
      srv = GetCPUHandle(FIRST_TEXDISPLAY_SRV);
      // YUV second plane is in slot 10
      srv.ptr += 10 * sizeof(D3D12Descriptor);
      m_pDevice->CreateShaderResourceView(resource, &altSRVDesc, srv);
    }
    else
    {
      srv.ptr += sizeof(D3D12Descriptor);
      m_pDevice->CreateShaderResourceView(resource, &altSRVDesc, srv);
    }
  }
}

bool D3D12Replay::RenderTexture(TextureDisplay cfg)
{
  return RenderTextureInternal(m_OutputWindows[m_CurrentOutputWindow].rtv, cfg,
                               eTexDisplay_BlendAlpha);
}

bool D3D12Replay::RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                                        TexDisplayFlags flags)
{
  const bool blendAlpha = (flags & eTexDisplay_BlendAlpha) != 0;

  ID3D12Resource *resource = WrappedID3D12Resource1::GetList()[cfg.resourceId];

  if(resource == NULL)
    return false;

  TexDisplayVSCBuffer vertexData = {};
  TexDisplayPSCBuffer pixelData = {};
  HeatmapData heatmapData = {};

  {
    if(cfg.overlay == DebugOverlay::QuadOverdrawDraw || cfg.overlay == DebugOverlay::QuadOverdrawPass)
    {
      heatmapData.HeatmapMode = HEATMAP_LINEAR;
    }
    else if(cfg.overlay == DebugOverlay::TriangleSizeDraw ||
            cfg.overlay == DebugOverlay::TriangleSizePass)
    {
      heatmapData.HeatmapMode = HEATMAP_TRISIZE;
    }

    if(heatmapData.HeatmapMode)
    {
      memcpy(heatmapData.ColorRamp, colorRamp, sizeof(colorRamp));

      RDCCOMPILE_ASSERT(sizeof(heatmapData.ColorRamp) == sizeof(colorRamp),
                        "C++ color ramp array is not the same size as the shader array");
    }
  }

  float x = cfg.xOffset;
  float y = cfg.yOffset;

  vertexData.Position.x = x * (2.0f / m_OutputWidth);
  vertexData.Position.y = -y * (2.0f / m_OutputHeight);

  vertexData.ScreenAspect.x = m_OutputHeight / m_OutputWidth;
  vertexData.ScreenAspect.y = 1.0f;

  vertexData.TextureResolution.x = 1.0f / vertexData.ScreenAspect.x;
  vertexData.TextureResolution.y = 1.0f;

  if(cfg.rangeMax <= cfg.rangeMin)
    cfg.rangeMax += 0.00001f;

  pixelData.Channels.x = cfg.red ? 1.0f : 0.0f;
  pixelData.Channels.y = cfg.green ? 1.0f : 0.0f;
  pixelData.Channels.z = cfg.blue ? 1.0f : 0.0f;
  pixelData.Channels.w = cfg.alpha ? 1.0f : 0.0f;

  pixelData.RangeMinimum = cfg.rangeMin;
  pixelData.InverseRangeSize = 1.0f / (cfg.rangeMax - cfg.rangeMin);

  if(_isnan(pixelData.InverseRangeSize) || !_finite(pixelData.InverseRangeSize))
  {
    pixelData.InverseRangeSize = FLT_MAX;
  }

  pixelData.WireframeColour.x = cfg.hdrMultiplier;
  pixelData.WireframeColour.y = cfg.decodeYUV ? 1.0f : 0.0f;

  pixelData.RawOutput = cfg.rawOutput ? 1 : 0;

  pixelData.FlipY = cfg.flipY ? 1 : 0;

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

  vertexData.TextureResolution.x *= tex_x / m_OutputWidth;
  vertexData.TextureResolution.y *= tex_y / m_OutputHeight;

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
    float xscale = m_OutputWidth / tex_x;
    float yscale = m_OutputHeight / tex_y;

    vertexData.Scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      vertexData.Position.x = 0;
      vertexData.Position.y = tex_y * vertexData.Scale / m_OutputHeight - 1.0f;
    }
    else
    {
      vertexData.Position.y = 0;
      vertexData.Position.x = 1.0f - tex_x * vertexData.Scale / m_OutputWidth;
    }
  }

  vertexData.Scale *= 2.0f;    // viewport is -1 -> 1

  pixelData.MipLevel = (float)cfg.mip;
  pixelData.OutputDisplayFormat = RESTYPE_TEX2D;
  pixelData.Slice = float(RDCCLAMP(cfg.sliceFace, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    pixelData.Slice = float(cfg.sliceFace >> cfg.mip);

  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, cfg.typeHint, resType, barriers);

  pixelData.OutputDisplayFormat = resType;

  if(cfg.overlay == DebugOverlay::NaN)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;

  if(cfg.overlay == DebugOverlay::Clipping)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;

  if(IsUIntFormat(resourceDesc.Format))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
  else if(IsIntFormat(resourceDesc.Format))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;

  if(!IsSRGBFormat(resourceDesc.Format) && cfg.linearDisplayAsGamma)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

  Vec4u YUVDownsampleRate = {}, YUVAChannels = {};

  GetYUVShaderParameters(resourceDesc.Format, YUVDownsampleRate, YUVAChannels);

  pixelData.YUVDownsampleRate = YUVDownsampleRate;
  pixelData.YUVAChannels = YUVAChannels;

  ID3D12PipelineState *customPSO = NULL;

  D3D12_GPU_VIRTUAL_ADDRESS psCBuf = 0;

  if(cfg.customShaderId != ResourceId())
  {
    WrappedID3D12Shader *shader =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(cfg.customShaderId);

    if(shader == NULL)
      return false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.pRootSignature = m_TexRender.RootSig;
    pipeDesc.VS.BytecodeLength = m_TexRender.VS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = m_TexRender.VS->GetBufferPointer();
    pipeDesc.PS = shader->GetDesc();
    pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeDesc.SampleMask = 0xFFFFFFFF;
    pipeDesc.SampleDesc.Count = 1;
    pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeDesc.NumRenderTargets = 1;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                        (void **)&customPSO);
    if(FAILED(hr))
      return false;

    DXBC::DXBCFile *dxbc = shader->GetDXBC();

    RDCASSERT(dxbc);
    RDCASSERT(dxbc->m_Type == D3D11_ShaderType_Pixel);

    for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
    {
      const DXBC::CBuffer &cbuf = dxbc->m_CBuffers[i];
      if(cbuf.name == "$Globals")
      {
        float *cbufData = new float[cbuf.descriptor.byteSize / sizeof(float) + 1];
        byte *byteData = (byte *)cbufData;

        for(size_t v = 0; v < cbuf.variables.size(); v++)
        {
          const DXBC::CBufferVariable &var = cbuf.variables[v];

          if(var.name == "RENDERDOC_TexDim")
          {
            if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 4 &&
               var.type.descriptor.type == DXBC::VARTYPE_UINT)
            {
              uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

              d[0] = (uint32_t)resourceDesc.Width;
              d[1] = resourceDesc.Height;
              d[2] = resourceDesc.DepthOrArraySize;
              d[3] = resourceDesc.MipLevels;
              if(resourceDesc.MipLevels == 0)
                d[3] = CalcNumMips(
                    d[1], d[2],
                    resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? d[3] : 1);
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected uint4: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_YUVDownsampleRate")
          {
            Vec4u *d = (Vec4u *)(byteData + var.descriptor.offset);

            *d = YUVDownsampleRate;
          }
          else if(var.name == "RENDERDOC_YUVAChannels")
          {
            Vec4u *d = (Vec4u *)(byteData + var.descriptor.offset);

            *d = YUVAChannels;
          }
          else if(var.name == "RENDERDOC_SelectedMip")
          {
            if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
               var.type.descriptor.type == DXBC::VARTYPE_UINT)
            {
              uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

              d[0] = cfg.mip;
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_SelectedSliceFace")
          {
            if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
               var.type.descriptor.type == DXBC::VARTYPE_UINT)
            {
              uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

              d[0] = cfg.sliceFace;
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_SelectedSample")
          {
            if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
               var.type.descriptor.type == DXBC::VARTYPE_INT)
            {
              int32_t *d = (int32_t *)(byteData + var.descriptor.offset);

              d[0] = cfg.sampleIdx;
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected int: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_TextureType")
          {
            if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
               var.type.descriptor.type == DXBC::VARTYPE_UINT)
            {
              uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

              d[0] = resType;
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                      var.name.c_str());
            }
          }
          else
          {
            RDCWARN("Custom shader: Variable not recognised: %s", var.name.c_str());
          }
        }

        psCBuf = GetDebugManager()->UploadConstants(cbufData, cbuf.descriptor.byteSize);

        SAFE_DELETE_ARRAY(cbufData);
      }
    }
  }
  else
  {
    psCBuf = GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));
  }

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)m_OutputWidth, (float)m_OutputHeight, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, (LONG)viewport.Width, (LONG)viewport.Height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    if(customPSO)
    {
      list->SetPipelineState(customPSO);
    }
    else if(cfg.rawOutput || !blendAlpha || cfg.customShaderId != ResourceId())
    {
      if(flags & eTexDisplay_F32Render)
        list->SetPipelineState(m_TexRender.F32Pipe);
      else if(flags & eTexDisplay_F16Render)
        list->SetPipelineState(m_TexRender.F16Pipe);
      else if(flags & eTexDisplay_LinearRender)
        list->SetPipelineState(m_TexRender.LinearPipe);
      else
        list->SetPipelineState(m_TexRender.SRGBPipe);
    }
    else
    {
      list->SetPipelineState(m_TexRender.BlendPipe);
    }

    list->SetGraphicsRootSignature(m_TexRender.RootSig);

    GetDebugManager()->SetDescriptorHeaps(list, true, true);

    list->SetGraphicsRootConstantBufferView(
        0, GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData)));
    list->SetGraphicsRootConstantBufferView(1, psCBuf);
    list->SetGraphicsRootConstantBufferView(
        2, GetDebugManager()->UploadConstants(&heatmapData, sizeof(heatmapData)));
    list->SetGraphicsRootDescriptorTable(3, GetDebugManager()->GetGPUHandle(FIRST_TEXDISPLAY_SRV));
    list->SetGraphicsRootDescriptorTable(4, GetDebugManager()->GetGPUHandle(FIRST_SAMP));

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(4, 1, 0, 0);

    // transition back to where they were
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();

    SAFE_RELEASE(customPSO);
  }

  return true;
}
