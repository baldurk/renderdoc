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

#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"

#include "data/hlsl/hlsl_cbuffers.h"

D3D11DebugManager::CacheElem &D3D11DebugManager::GetCachedElem(ResourceId id, CompType typeHint,
                                                               bool raw)
{
  for(auto it = m_ShaderItemCache.begin(); it != m_ShaderItemCache.end(); ++it)
  {
    if(it->id == id && it->typeHint == typeHint && it->raw == raw)
      return *it;
  }

  if(m_ShaderItemCache.size() >= NUM_CACHED_SRVS)
  {
    CacheElem &elem = m_ShaderItemCache.back();
    elem.Release();
    m_ShaderItemCache.pop_back();
  }

  m_ShaderItemCache.push_front(CacheElem(id, typeHint, raw));
  return m_ShaderItemCache.front();
}

TextureShaderDetails D3D11DebugManager::GetShaderDetails(ResourceId id, CompType typeHint,
                                                         bool rawOutput)
{
  TextureShaderDetails details;
  HRESULT hr = S_OK;

  bool foundResource = false;

  CacheElem &cache = GetCachedElem(id, typeHint, rawOutput);

  bool msaaDepth = false;

  DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

  if(WrappedID3D11Texture1D::m_TextureList.find(id) != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *wrapTex1D =
        (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[id].m_Texture;
    TextureDisplayType mode = WrappedID3D11Texture1D::m_TextureList[id].m_Type;

    foundResource = true;

    details.texType = eTexType_1D;

    if(mode == TEXDISPLAY_DEPTH_TARGET)
      details.texType = eTexType_Depth;

    D3D11_TEXTURE1D_DESC desc1d = {0};
    wrapTex1D->GetDesc(&desc1d);

    details.texFmt = desc1d.Format;
    details.texWidth = desc1d.Width;
    details.texHeight = 1;
    details.texDepth = 1;
    details.texArraySize = desc1d.ArraySize;
    details.texMips = desc1d.MipLevels;

    srvFormat = GetTypedFormat(details.texFmt, typeHint);

    details.srvResource = wrapTex1D;

    if(mode == TEXDISPLAY_INDIRECT_VIEW || mode == TEXDISPLAY_DEPTH_TARGET)
    {
      D3D11_TEXTURE1D_DESC desc = desc1d;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      if(mode == TEXDISPLAY_DEPTH_TARGET)
        desc.Format = GetTypelessFormat(desc.Format);

      if(!cache.created)
      {
        ID3D11Texture1D *tmp = NULL;
        hr = m_pDevice->CreateTexture1D(&desc, NULL, &tmp);

        if(FAILED(hr))
        {
          RDCERR("Failed to create temporary Texture1D HRESULT: %s", ToStr(hr).c_str());
        }

        cache.srvResource = tmp;
      }

      details.previewCopy = cache.srvResource;

      m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

      details.srvResource = details.previewCopy;
    }
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(id) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *wrapTex2D =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[id].m_Texture;
    TextureDisplayType mode = WrappedID3D11Texture2D1::m_TextureList[id].m_Type;

    foundResource = true;

    details.texType = eTexType_2D;

    D3D11_TEXTURE2D_DESC desc2d = {0};
    wrapTex2D->GetDesc(&desc2d);

    details.texFmt = desc2d.Format;
    details.texWidth = desc2d.Width;
    details.texHeight = desc2d.Height;
    details.texDepth = 1;
    details.texArraySize = desc2d.ArraySize;
    details.texMips = desc2d.MipLevels;
    details.sampleCount = RDCMAX(1U, desc2d.SampleDesc.Count);
    details.sampleQuality = desc2d.SampleDesc.Quality;

    if(desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0)
    {
      details.texType = eTexType_2DMS;
    }

    if(mode == TEXDISPLAY_DEPTH_TARGET || IsDepthFormat(details.texFmt))
    {
      details.texType = eTexType_Depth;
      details.texFmt = GetTypedFormat(details.texFmt, typeHint);
    }

    // backbuffer is always interpreted as SRGB data regardless of format specified:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh972627(v=vs.85).aspx
    //
    // "The app must always place sRGB data into back buffers with integer-valued formats
    // to present the sRGB data to the screen, even if the data doesn't have this format
    // modifier in its format name."
    //
    // This essentially corrects for us always declaring an SRGB render target for our
    // output displays, as any app with a non-SRGB backbuffer would be incorrectly converted
    // unless we read out SRGB here.
    //
    // However when picking a pixel we want the actual value stored, not the corrected perceptual
    // value so for raw output we don't do this. This does my head in, it really does.
    if(wrapTex2D->m_RealDescriptor)
    {
      if(rawOutput)
        details.texFmt = wrapTex2D->m_RealDescriptor->Format;
      else
        details.texFmt = GetSRGBFormat(wrapTex2D->m_RealDescriptor->Format);
    }

    srvFormat = GetTypedFormat(details.texFmt, typeHint);

    details.srvResource = wrapTex2D;

    if(mode == TEXDISPLAY_INDIRECT_VIEW || mode == TEXDISPLAY_DEPTH_TARGET ||
       desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0)
    {
      D3D11_TEXTURE2D_DESC desc = desc2d;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      if(mode == TEXDISPLAY_DEPTH_TARGET)
      {
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Format = GetTypelessFormat(desc.Format);
      }
      else
      {
        desc.Format = srvFormat;
      }

      if(!cache.created)
      {
        ID3D11Texture2D *tmp = NULL;
        hr = m_pDevice->CreateTexture2D(&desc, NULL, &tmp);

        if(FAILED(hr))
        {
          RDCERR("Failed to create temporary Texture2D HRESULT: %s", ToStr(hr).c_str());
        }

        cache.srvResource = tmp;
      }

      details.previewCopy = cache.srvResource;

      if((desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0) &&
         mode == TEXDISPLAY_DEPTH_TARGET)
        msaaDepth = true;

      m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

      details.srvResource = details.previewCopy;
    }
  }
  else if(WrappedID3D11Texture3D1::m_TextureList.find(id) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *wrapTex3D =
        (WrappedID3D11Texture3D1 *)WrappedID3D11Texture3D1::m_TextureList[id].m_Texture;
    TextureDisplayType mode = WrappedID3D11Texture3D1::m_TextureList[id].m_Type;

    foundResource = true;

    details.texType = eTexType_3D;

    D3D11_TEXTURE3D_DESC desc3d = {0};
    wrapTex3D->GetDesc(&desc3d);

    details.texFmt = desc3d.Format;
    details.texWidth = desc3d.Width;
    details.texHeight = desc3d.Height;
    details.texDepth = desc3d.Depth;
    details.texArraySize = 1;
    details.texMips = desc3d.MipLevels;

    srvFormat = GetTypedFormat(details.texFmt, typeHint);

    details.srvResource = wrapTex3D;

    if(mode == TEXDISPLAY_INDIRECT_VIEW)
    {
      D3D11_TEXTURE3D_DESC desc = desc3d;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      if(IsUIntFormat(srvFormat) || IsIntFormat(srvFormat))
        desc.Format = GetTypelessFormat(desc.Format);

      if(!cache.created)
      {
        ID3D11Texture3D *tmp = NULL;
        hr = m_pDevice->CreateTexture3D(&desc, NULL, &tmp);

        if(FAILED(hr))
        {
          RDCERR("Failed to create temporary Texture3D HRESULT: %s", ToStr(hr).c_str());
        }

        cache.srvResource = tmp;
      }

      details.previewCopy = cache.srvResource;

      m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

      details.srvResource = details.previewCopy;
    }
  }

  if(!foundResource)
  {
    RDCERR("bad texture trying to be displayed");
    return TextureShaderDetails();
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc[eTexType_Max];

  srvDesc[eTexType_1D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
  srvDesc[eTexType_1D].Texture1DArray.ArraySize = details.texArraySize;
  srvDesc[eTexType_1D].Texture1DArray.FirstArraySlice = 0;
  srvDesc[eTexType_1D].Texture1DArray.MipLevels = details.texMips;
  srvDesc[eTexType_1D].Texture1DArray.MostDetailedMip = 0;

  srvDesc[eTexType_2D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc[eTexType_2D].Texture2DArray.ArraySize = details.texArraySize;
  srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice = 0;
  srvDesc[eTexType_2D].Texture2DArray.MipLevels = details.texMips;
  srvDesc[eTexType_2D].Texture2DArray.MostDetailedMip = 0;

  srvDesc[eTexType_2DMS].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
  srvDesc[eTexType_2DMS].Texture2DMSArray.ArraySize = details.texArraySize;
  srvDesc[eTexType_2DMS].Texture2DMSArray.FirstArraySlice = 0;

  srvDesc[eTexType_Stencil] = srvDesc[eTexType_Depth] = srvDesc[eTexType_2D];

  srvDesc[eTexType_3D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
  srvDesc[eTexType_3D].Texture3D.MipLevels = details.texMips;
  srvDesc[eTexType_3D].Texture3D.MostDetailedMip = 0;

  for(int i = 0; i < eTexType_Max; i++)
    srvDesc[i].Format = srvFormat;

  if(details.texType == eTexType_Depth)
  {
    switch(details.texFmt)
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      }
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_UNKNOWN;
        break;
      }
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
      }
      case DXGI_FORMAT_R16_FLOAT:
      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R16_UNORM;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_UNKNOWN;
        break;
      }
      default: break;
    }
  }

  if(IsYUVFormat(srvFormat))
  {
    // assume YUV textures are 2D or 2D arrays
    RDCASSERT(details.texType == eTexType_2D);

    srvDesc[details.texType].Format = GetYUVViewPlane0Format(srvFormat);

    GetYUVShaderParameters(srvFormat, details.YUVDownsampleRate, details.YUVAChannels);
  }

  if(msaaDepth)
  {
    srvDesc[eTexType_Stencil].ViewDimension = srvDesc[eTexType_Depth].ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;

    srvDesc[eTexType_Depth].Texture2DMSArray.ArraySize =
        srvDesc[eTexType_2D].Texture2DArray.ArraySize;
    srvDesc[eTexType_Stencil].Texture2DMSArray.ArraySize =
        srvDesc[eTexType_2D].Texture2DArray.ArraySize;
    srvDesc[eTexType_Depth].Texture2DMSArray.FirstArraySlice =
        srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice;
    srvDesc[eTexType_Stencil].Texture2DMSArray.FirstArraySlice =
        srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice;
  }

  if(!cache.created)
  {
    hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[details.texType],
                                             &cache.srv[0]);

    if(FAILED(hr))
      RDCERR("Failed to create cache SRV 0, type %d HRESULT: %s", details.texType, ToStr(hr).c_str());
  }

  details.srv[details.texType] = cache.srv[0];

  if(IsYUVFormat(srvFormat))
  {
    srvDesc[details.texType].Format = GetYUVViewPlane1Format(srvFormat);

    if(srvDesc[details.texType].Format != DXGI_FORMAT_UNKNOWN)
    {
      if(!cache.created)
      {
        hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[details.texType],
                                                 &cache.srv[1]);

        if(FAILED(hr))
          RDCERR("Failed to create cache YUV SRV 1, type %d HRESULT: %s", details.texType,
                 ToStr(hr).c_str());
      }

      details.srv[eTexType_YUV] = cache.srv[1];
    }
  }

  if(details.texType == eTexType_Depth && srvDesc[eTexType_Stencil].Format != DXGI_FORMAT_UNKNOWN)
  {
    if(!cache.created)
    {
      hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[eTexType_Stencil],
                                               &cache.srv[1]);

      if(FAILED(hr))
        RDCERR("Failed to create cache SRV 1, type %d HRESULT: %s", details.texType,
               ToStr(hr).c_str());
    }

    details.srv[eTexType_Stencil] = cache.srv[1];

    details.texType = eTexType_Stencil;
  }

  if(msaaDepth)
  {
    if(details.texType == eTexType_Depth)
      details.texType = eTexType_DepthMS;
    if(details.texType == eTexType_Stencil)
      details.texType = eTexType_StencilMS;

    details.srv[eTexType_Depth] = NULL;
    details.srv[eTexType_Stencil] = NULL;
    details.srv[eTexType_DepthMS] = cache.srv[0];
    details.srv[eTexType_StencilMS] = cache.srv[1];
  }

  cache.created = true;

  return details;
}

bool D3D11Replay::RenderTextureInternal(TextureDisplay cfg, bool blendAlpha)
{
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

  vertexData.ScreenAspect.x =
      (m_OutputHeight / m_OutputWidth);    // 0.5 = character width / character height
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

  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(cfg.resourceId, cfg.typeHint,
                                                                     cfg.rawOutput ? true : false);

  int sampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, details.sampleCount - 1);

  // hacky resolve
  if(cfg.sampleIdx == ~0U)
    sampleIdx = -int(details.sampleCount);

  pixelData.SampleIdx = sampleIdx;

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  if(details.texFmt == DXGI_FORMAT_A8_UNORM && cfg.scale <= 0.0f)
  {
    pixelData.Channels.x = pixelData.Channels.y = pixelData.Channels.z = 0.0f;
    pixelData.Channels.w = 1.0f;
  }

  float tex_x = float(details.texWidth);
  float tex_y = float(details.texType == eTexType_1D ? 100 : details.texHeight);

  vertexData.TextureResolution.x *= tex_x / m_OutputWidth;
  vertexData.TextureResolution.y *= tex_y / m_OutputHeight;

  pixelData.TextureResolutionPS.x = float(RDCMAX(1U, details.texWidth >> cfg.mip));
  pixelData.TextureResolutionPS.y = float(RDCMAX(1U, details.texHeight >> cfg.mip));
  pixelData.TextureResolutionPS.z = float(RDCMAX(1U, details.texDepth >> cfg.mip));

  if(details.texArraySize > 1 && details.texType != eTexType_3D)
    pixelData.TextureResolutionPS.z = float(details.texArraySize);

  vertexData.Scale = cfg.scale;
  pixelData.ScalePS = cfg.scale;

  pixelData.YUVDownsampleRate = details.YUVDownsampleRate;
  pixelData.YUVAChannels = details.YUVAChannels;

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

  ID3D11PixelShader *customPS = NULL;
  ID3D11Buffer *customBuff = NULL;

  if(cfg.customShaderId != ResourceId())
  {
    auto it = WrappedShader::m_ShaderList.find(cfg.customShaderId);

    if(it != WrappedShader::m_ShaderList.end())
    {
      auto dxbc = it->second->GetDXBC();

      RDCASSERT(dxbc);
      RDCASSERT(dxbc->m_Type == D3D11_ShaderType_Pixel);

      if(m_pDevice->GetResourceManager()->HasLiveResource(cfg.customShaderId))
      {
        WrappedID3D11Shader<ID3D11PixelShader> *wrapped =
            (WrappedID3D11Shader<ID3D11PixelShader> *)m_pDevice->GetResourceManager()->GetLiveResource(
                cfg.customShaderId);

        customPS = wrapped;

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

                  d[0] = details.texWidth;
                  d[1] = details.texHeight;
                  d[2] = details.texType == eTexType_3D ? details.texDepth : details.texArraySize;
                  d[3] = details.texMips;
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

                *d = details.YUVDownsampleRate;
              }
              else if(var.name == "RENDERDOC_YUVAChannels")
              {
                Vec4u *d = (Vec4u *)(byteData + var.descriptor.offset);

                *d = details.YUVAChannels;
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

                  d[0] = details.texType;
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

            customBuff = GetDebugManager()->MakeCBuffer(cbufData, cbuf.descriptor.byteSize);

            SAFE_DELETE_ARRAY(cbufData);
          }
        }
      }
    }
  }

  vertexData.Scale *= 2.0f;    // viewport is -1 -> 1

  pixelData.MipLevel = (float)cfg.mip;
  pixelData.OutputDisplayFormat = RESTYPE_TEX2D;
  pixelData.Slice = float(RDCCLAMP(cfg.sliceFace, 0U, details.texArraySize - 1));

  if(details.texType == eTexType_3D)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX3D;
    pixelData.Slice = float(cfg.sliceFace >> cfg.mip);
  }
  else if(details.texType == eTexType_1D)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX1D;
  }
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
  }
  else if(details.texType == eTexType_2DMS)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX2D_MS;
  }

  if(cfg.overlay == DebugOverlay::NaN)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;
  }

  if(cfg.overlay == DebugOverlay::Clipping)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;
  }

  int srvOffset = 0;

  if(IsUIntFormat(details.texFmt) ||
     (IsTypelessFormat(details.texFmt) && cfg.typeHint == CompType::UInt))
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
    srvOffset = 10;
  }
  if(IsIntFormat(details.texFmt) ||
     (IsTypelessFormat(details.texFmt) && cfg.typeHint == CompType::SInt))
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;
    srvOffset = 20;
  }
  if(!IsSRGBFormat(details.texFmt) && cfg.linearDisplayAsGamma)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;
  }

  ID3D11Buffer *vsCBuffer = GetDebugManager()->MakeCBuffer(&vertexData, sizeof(vertexData));
  ID3D11Buffer *psCBuffer = GetDebugManager()->MakeCBuffer(&pixelData, sizeof(pixelData));
  ID3D11Buffer *psHeatCBuffer = GetDebugManager()->MakeCBuffer(&heatmapData, sizeof(heatmapData));

  // can't just clear state because we need to keep things like render targets.
  {
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pImmediateContext->IASetInputLayout(NULL);

    m_pImmediateContext->VSSetShader(m_TexRender.TexDisplayVS, NULL, 0);
    m_pImmediateContext->VSSetConstantBuffers(0, 1, &vsCBuffer);

    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);

    m_pImmediateContext->RSSetState(m_General.RasterState);

    if(customPS == NULL)
    {
      m_pImmediateContext->PSSetShader(m_TexRender.TexDisplayPS, NULL, 0);
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuffer);
      m_pImmediateContext->PSSetConstantBuffers(1, 1, &psHeatCBuffer);
    }
    else
    {
      m_pImmediateContext->PSSetShader(customPS, NULL, 0);
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &customBuff);
    }

    ID3D11UnorderedAccessView *NullUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
    UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT];
    memset(&UAV_keepcounts[0], 0xff, sizeof(UAV_keepcounts));
    const UINT numUAVs =
        m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;

    m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, NullUAVs, UAV_keepcounts);

    m_pImmediateContext->PSSetShaderResources(srvOffset, eTexType_Max, details.srv);

    ID3D11SamplerState *samps[] = {m_TexRender.PointSampState, m_TexRender.LinearSampState};
    m_pImmediateContext->PSSetSamplers(0, 2, samps);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if(cfg.rawOutput || !blendAlpha || cfg.customShaderId != ResourceId())
      m_pImmediateContext->OMSetBlendState(NULL, factor, 0xffffffff);
    else
      m_pImmediateContext->OMSetBlendState(m_TexRender.BlendState, factor, 0xffffffff);

    m_pImmediateContext->Draw(4, 0);
  }

  return true;
}
