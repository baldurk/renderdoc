/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "d3d12_replay.h"

#include "data/hlsl/hlsl_cbuffers.h"

void D3D12DebugManager::PrepareTextureSampling(ID3D12Resource *resource, CompType typeCast,
                                               int &resType, BarrierSet &barrierSet)
{
  int srvOffset = 0;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = GetTypedFormat(resourceDesc.Format, typeCast);
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

      if(IsDepthFormat(srvDesc.Format))
        srvOffset = RESTYPE_DEPTH_MS;
    }
    else
    {
      srvOffset = RESTYPE_TEX2D;
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2D.MipLevels = ~0U;
      srvDesc.Texture2DArray.ArraySize = ~0U;

      if(IsDepthFormat(srvDesc.Format))
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
  if(IsDepthAndStencilFormat(srvDesc.Format))
    resType++;

  if(IsUIntFormat(srvDesc.Format))
    srvOffset += 10;
  if(IsIntFormat(srvDesc.Format))
    srvOffset += 20;

  bool copy = false;

  D3D12_SHADER_RESOURCE_VIEW_DESC altSRVDesc = {};

  // for non-typeless depth formats, we need to copy to a typeless resource for read
  if(IsDepthFormat(srvDesc.Format) && GetTypelessFormat(srvDesc.Format) != srvDesc.Format)
  {
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

  if(IsYUVFormat(srvDesc.Format))
  {
    altSRVDesc = srvDesc;
    srvDesc.Format = GetYUVViewPlane0Format(srvDesc.Format);
    altSRVDesc.Format = GetYUVViewPlane1Format(srvDesc.Format);

    // assume YUV textures are 2D or 2D arrays
    RDCASSERT(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

    // the second SRV, if used, is always for the second plane
    altSRVDesc.Texture2DArray.PlaneSlice = 1;
  }

  // even for non-copies, we need to make two SRVs to sample stencil as well
  if(IsDepthAndStencilFormat(srvDesc.Format) && altSRVDesc.Format == DXGI_FORMAT_UNKNOWN)
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
      default: break;
    }
  }

  if(altSRVDesc.Format != DXGI_FORMAT_UNKNOWN && !IsYUVFormat(srvDesc.Format))
  {
    D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
    formatInfo.Format = srvDesc.Format;
    m_pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

    if(formatInfo.PlaneCount > 1 && altSRVDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      altSRVDesc.Texture2DArray.PlaneSlice = 1;
  }

  barrierSet.Configure(resource, m_pDevice->GetSubresourceStates(GetResID(resource)),
                       copy ? BarrierSet::CopySourceAccess : BarrierSet::SRVAccess);

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

      if(FAILED(hr))
      {
        RDCERR("Couldn't create display texture");
        return;
      }

      m_TexResource->SetName(L"m_TexResource");
    }

    ID3D12GraphicsCommandListX *list = m_pDevice->GetNewList();
    if(!list)
      return;

    // prepare real resource for copying
    barrierSet.Apply(list);

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

    barrierSet.Unapply(list);

    // don't do any barriers outside in the source function
    barrierSet.clear();

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
    if(IsYUVFormat(srvDesc.Format))
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
  m_OutputViewport = {0, 0, (float)m_OutputWidth, (float)m_OutputHeight, 0.0f, 1.0f};
  return RenderTextureInternal(m_OutputWindows[m_CurrentOutputWindow].rtv, cfg,
                               eTexDisplay_BlendAlpha);
}

bool D3D12Replay::RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                                        TexDisplayFlags flags)
{
  const bool blendAlpha = (flags & eTexDisplay_BlendAlpha) != 0;

  ID3D12Resource *resource = NULL;

  {
    auto it = m_pDevice->GetResourceList().find(cfg.resourceId);
    if(it != m_pDevice->GetResourceList().end())
      resource = it->second;
  }

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

  if(cfg.rangeMax <= cfg.rangeMin)
    cfg.rangeMax += 0.00001f;

  pixelData.Channels.x = cfg.red ? 1.0f : 0.0f;
  pixelData.Channels.y = cfg.green ? 1.0f : 0.0f;
  pixelData.Channels.z = cfg.blue ? 1.0f : 0.0f;
  pixelData.Channels.w = cfg.alpha ? 1.0f : 0.0f;

  pixelData.RangeMinimum = cfg.rangeMin;
  pixelData.InverseRangeSize = 1.0f / (cfg.rangeMax - cfg.rangeMin);

  if(!RDCISFINITE(pixelData.InverseRangeSize))
  {
    pixelData.InverseRangeSize = FLT_MAX;
  }

  pixelData.WireframeColour.x = cfg.hdrMultiplier;
  pixelData.WireframeColour.y = cfg.decodeYUV ? 1.0f : 0.0f;

  pixelData.RawOutput = cfg.rawOutput ? 1 : 0;

  pixelData.FlipY = cfg.flipY ? 1 : 0;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  pixelData.SampleIdx = (int)RDCCLAMP(cfg.subresource.sample, 0U, resourceDesc.SampleDesc.Count - 1);

  // hacky resolve
  if(cfg.subresource.sample == ~0U)
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

  pixelData.TextureResolutionPS.x =
      float(RDCMAX(1U, uint32_t(resourceDesc.Width >> cfg.subresource.mip)));
  pixelData.TextureResolutionPS.y =
      float(RDCMAX(1U, uint32_t(resourceDesc.Height >> cfg.subresource.mip)));
  pixelData.TextureResolutionPS.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> cfg.subresource.mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    pixelData.TextureResolutionPS.z = float(resourceDesc.DepthOrArraySize);

  pixelData.ScalePS = cfg.scale;

  if(cfg.scale <= 0.0f)
  {
    float xscale = m_OutputWidth / tex_x;
    float yscale = m_OutputHeight / tex_y;

    cfg.scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      vertexData.Position.x = 0;
      vertexData.Position.y = tex_y * cfg.scale / m_OutputHeight - 1.0f;
    }
    else
    {
      vertexData.Position.y = 0;
      vertexData.Position.x = 1.0f - tex_x * cfg.scale / m_OutputWidth;
    }
  }

  // normalisation factor for output * selected scale * viewport scale
  vertexData.VertexScale.x = (tex_x / m_OutputWidth) * cfg.scale * 2.0f;
  vertexData.VertexScale.y = (tex_y / m_OutputHeight) * cfg.scale * 2.0f;

  pixelData.MipLevel = (float)cfg.subresource.mip;

  DXGI_FORMAT fmt = GetTypedFormat(resourceDesc.Format, cfg.typeCast);

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    float slice =
        float(RDCCLAMP(cfg.subresource.slice, 0U,
                       uint32_t((resourceDesc.DepthOrArraySize >> cfg.subresource.mip) - 1)));

    // when sampling linearly, we need to add half a pixel to ensure we only sample the desired
    // slice
    if(cfg.subresource.mip == 0 && cfg.scale < 1.0f && !IsUIntFormat(fmt) && !IsIntFormat(fmt))
      slice += 0.5f;
    else
      slice += 0.001f;

    pixelData.Slice = slice;
  }
  else
  {
    pixelData.Slice = float(
        RDCCLAMP(cfg.subresource.slice, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)) + 0.001f);
  }

  BarrierSet barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, cfg.typeCast, resType, barriers);

  pixelData.OutputDisplayFormat = resType;

  if(cfg.overlay == DebugOverlay::NaN)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;

  if(cfg.overlay == DebugOverlay::Clipping)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;

  if(IsUIntFormat(fmt))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
  else if(IsIntFormat(fmt))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;

  // Check both the resource format and view format for sRGB
  if(!IsSRGBFormat(resourceDesc.Format) && cfg.typeCast != CompType::UNormSRGB &&
     cfg.linearDisplayAsGamma)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

  Vec4u YUVDownsampleRate = {}, YUVAChannels = {};

  GetYUVShaderParameters(resourceDesc.Format, YUVDownsampleRate, YUVAChannels);

  pixelData.YUVDownsampleRate = YUVDownsampleRate;
  pixelData.YUVAChannels = YUVAChannels;

  ID3D12PipelineState *customPSO = NULL;

  D3D12_GPU_VIRTUAL_ADDRESS psCBuf = 0;
  D3D12_GPU_VIRTUAL_ADDRESS secondCBuf =
      GetDebugManager()->UploadConstants(&heatmapData, sizeof(heatmapData));

  if(cfg.customShaderId != ResourceId())
  {
    WrappedID3D12Shader *shader =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(cfg.customShaderId);

    if(shader == NULL)
      return false;

    RD_CustomShader_CBuffer_Type customCBuffer = {};

    customCBuffer.TexDim.x = (uint32_t)resourceDesc.Width;
    customCBuffer.TexDim.y = resourceDesc.Height;
    customCBuffer.TexDim.z = resourceDesc.DepthOrArraySize;
    customCBuffer.TexDim.w = resourceDesc.MipLevels;
    customCBuffer.SelectedMip = cfg.subresource.mip;
    customCBuffer.SelectedSliceFace = cfg.subresource.slice;
    customCBuffer.SelectedSample = cfg.subresource.sample;
    if(cfg.subresource.sample == ~0U)
      customCBuffer.SelectedSample = -int(resourceDesc.SampleDesc.Count);
    customCBuffer.TextureType = (uint32_t)resType;
    customCBuffer.YUVDownsampleRate = YUVDownsampleRate;
    customCBuffer.YUVAChannels = YUVAChannels;
    customCBuffer.SelectedRange.x = cfg.rangeMin;
    customCBuffer.SelectedRange.y = cfg.rangeMax;

    psCBuf = GetDebugManager()->UploadConstants(&customCBuffer, sizeof(customCBuffer));

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
    pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                        (void **)&customPSO);
    if(FAILED(hr))
      return false;

    DXBC::DXBCContainer *dxbc = shader->GetDXBC();

    RDCASSERT(dxbc);
    RDCASSERT(dxbc->m_Type == DXBC::ShaderType::Pixel);

    for(size_t i = 0; i < dxbc->GetReflection()->CBuffers.size(); i++)
    {
      const DXBC::CBuffer &cbuf = dxbc->GetReflection()->CBuffers[i];
      if(cbuf.name == "$Globals")
      {
        float *cbufData = new float[cbuf.descriptor.byteSize / sizeof(float) + 1];
        byte *byteData = (byte *)cbufData;

        for(size_t v = 0; v < cbuf.variables.size(); v++)
        {
          const DXBC::CBufferVariable &var = cbuf.variables[v];

          if(var.name == "RENDERDOC_TexDim")
          {
            if(var.type.rows == 1 && var.type.cols == 4 && var.type.varType == VarType::UInt)
            {
              uint32_t *d = (uint32_t *)(byteData + var.offset);

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
            Vec4u *d = (Vec4u *)(byteData + var.offset);

            *d = YUVDownsampleRate;
          }
          else if(var.name == "RENDERDOC_YUVAChannels")
          {
            Vec4u *d = (Vec4u *)(byteData + var.offset);

            *d = YUVAChannels;
          }
          else if(var.name == "RENDERDOC_SelectedMip")
          {
            if(var.type.rows == 1 && var.type.cols == 1 && var.type.varType == VarType::UInt)
            {
              uint32_t *d = (uint32_t *)(byteData + var.offset);

              d[0] = cfg.subresource.mip;
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_SelectedSliceFace")
          {
            if(var.type.rows == 1 && var.type.cols == 1 && var.type.varType == VarType::UInt)
            {
              uint32_t *d = (uint32_t *)(byteData + var.offset);

              d[0] = cfg.subresource.slice;
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_SelectedSample")
          {
            if(var.type.rows == 1 && var.type.cols == 1 && var.type.varType == VarType::SInt)
            {
              int32_t *d = (int32_t *)(byteData + var.offset);

              d[0] = cfg.subresource.sample;
              if(cfg.subresource.sample == ~0U)
                d[0] = -int(resourceDesc.SampleDesc.Count);
            }
            else
            {
              RDCWARN("Custom shader: Variable recognised but type wrong, expected int: %s",
                      var.name.c_str());
            }
          }
          else if(var.name == "RENDERDOC_TextureType")
          {
            if(var.type.rows == 1 && var.type.cols == 1 && var.type.varType == VarType::UInt)
            {
              uint32_t *d = (uint32_t *)(byteData + var.offset);

              d[0] = resType;
            }
            else if(var.name == "RENDERDOC_SelectedRangeMin")
            {
              float *d = (float *)(byteData + var.offset);
              d[0] = cfg.rangeMin;
            }
            else if(var.name == "RENDERDOC_SelectedRangeMax")
            {
              float *d = (float *)(byteData + var.offset);
              d[0] = cfg.rangeMax;
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

        if(cbuf.reg == 0)
        {
          // with the prefix added, binding 0 should be 'reserved' for the modern cbuffer.
          // we can still make this work, but it's unexpected
          RDCWARN(
              "Unexpected globals cbuffer at binding 0, expected binding 1 after prefix cbuffer");
          psCBuf = GetDebugManager()->UploadConstants(cbufData, cbuf.descriptor.byteSize);
        }
        else if(cbuf.reg == 1)
        {
          secondCBuf = GetDebugManager()->UploadConstants(cbufData, cbuf.descriptor.byteSize);
        }
        else
        {
          RDCERR(
              "Globals cbuffer at binding %d, unexpected and not handled - these constants will be "
              "undefined",
              cbuf.reg);
        }

        SAFE_DELETE_ARRAY(cbufData);
      }
    }
  }
  else
  {
    psCBuf = GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));
  }

  {
    ID3D12GraphicsCommandListX *list = m_pDevice->GetNewList();
    if(!list)
      return false;

    barriers.Apply(list);

    list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

    list->RSSetViewports(1, &m_OutputViewport);

    D3D12_RECT scissor = {0, 0, (LONG)m_OutputViewport.Width, (LONG)m_OutputViewport.Height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    if(customPSO)
    {
      list->SetPipelineState(customPSO);
    }
    else if(flags & (eTexDisplay_RemapFloat | eTexDisplay_RemapUInt | eTexDisplay_RemapSInt))
    {
      int i = 0;
      if(flags & eTexDisplay_RemapFloat)
        i = 0;
      else if(flags & eTexDisplay_RemapUInt)
        i = 1;
      else if(flags & eTexDisplay_RemapSInt)
        i = 2;

      int f = 0;
      if(flags & eTexDisplay_32Render)
        f = 2;
      else if(flags & eTexDisplay_16Render)
        f = 1;
      else
        f = 0;

      list->SetPipelineState(m_TexRender.m_TexRemapPipe[f][i]);
    }
    else if(cfg.rawOutput || !blendAlpha || cfg.customShaderId != ResourceId())
    {
      if(flags & eTexDisplay_32Render)
        list->SetPipelineState(m_TexRender.F32Pipe);
      else if(flags & eTexDisplay_16Render)
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
    list->SetGraphicsRootConstantBufferView(2, secondCBuf);
    list->SetGraphicsRootDescriptorTable(3, GetDebugManager()->GetGPUHandle(FIRST_TEXDISPLAY_SRV));
    list->SetGraphicsRootDescriptorTable(4, GetDebugManager()->GetGPUHandle(FIRST_SAMP));

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(4, 1, 0, 0);

    barriers.Unapply(list);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();

    SAFE_RELEASE(customPSO);
  }

  return true;
}
