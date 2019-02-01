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

#include "d3d11_common.h"
#include "core/core.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"

WrappedID3D11Device *D3D11MarkerRegion::device;

D3D11MarkerRegion::D3D11MarkerRegion(const std::string &marker)
{
  D3D11MarkerRegion::Begin(marker);
}

D3D11MarkerRegion::~D3D11MarkerRegion()
{
  D3D11MarkerRegion::End();
}

void D3D11MarkerRegion::Set(const std::string &marker)
{
  if(device == NULL)
    return;

  ID3DUserDefinedAnnotation *annot = device->GetAnnotations();

  if(annot)
    annot->SetMarker(StringFormat::UTF82Wide(marker).c_str());
}

void D3D11MarkerRegion::Begin(const std::string &marker)
{
  if(device == NULL)
    return;

  ID3DUserDefinedAnnotation *annot = device->GetAnnotations();

  if(annot)
    annot->BeginEvent(StringFormat::UTF82Wide(marker).c_str());
}

void D3D11MarkerRegion::End()
{
  if(device == NULL)
    return;

  ID3DUserDefinedAnnotation *annot = device->GetAnnotations();

  if(annot)
    annot->EndEvent();
}

ResourceRange ResourceRange::Null = ResourceRange(ResourceRange::empty);

ResourceRange::ResourceRange(ID3D11ShaderResourceView *srv)
{
  minMip = minSlice = 0;
  depthReadOnly = false;
  stencilReadOnly = false;

  if(srv == NULL)
  {
    resource = NULL;
    maxMip = allMip;
    maxSlice = allSlice;
    fullRange = true;
    return;
  }

// in non-release make sure we always consistently check wrapped resources/views. Otherwise we could
// end up in a situation where we're comparing two resource ranges that do overlap, but one was
// constructed with the wrapped view and one wasn't - so they compare differently
#if ENABLED(RDOC_DEVEL)
  RDCASSERT(WrappedID3D11ShaderResourceView1::IsAlloc(srv));
#endif

  ID3D11Resource *res = NULL;
  srv->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = allMip, numSlices = allSlice;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
  srv->GetDesc(&srvd);

  // extract depth/stencil read only flags if appropriate
  {
    D3D11_RESOURCE_DIMENSION dim;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    fmt = srvd.Format;

    res->GetType(&dim);

    if(fmt == DXGI_FORMAT_UNKNOWN)
    {
      if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
      {
        D3D11_TEXTURE1D_DESC d;
        ((ID3D11Texture1D *)res)->GetDesc(&d);

        fmt = d.Format;
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      {
        D3D11_TEXTURE2D_DESC d;
        ((ID3D11Texture2D *)res)->GetDesc(&d);

        fmt = d.Format;
      }
    }

    if(fmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT || fmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
    {
      stencilReadOnly = true;
    }
    else if(fmt == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS || fmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
    {
      depthReadOnly = true;
    }
    else
    {
      fmt = GetTypelessFormat(fmt);

      // any format that could be depth-only, treat it as reading depth only.
      // this only applies for conflicts detected with the depth target.
      if(fmt == DXGI_FORMAT_R32_TYPELESS || fmt == DXGI_FORMAT_R16_TYPELESS)
      {
        depthReadOnly = true;
      }
    }
  }

  switch(srvd.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_TEXTURE1D:
      minMip = srvd.Texture1D.MostDetailedMip;
      numMips = srvd.Texture1D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
      minMip = srvd.Texture1DArray.MostDetailedMip;
      numMips = srvd.Texture1DArray.MipLevels;
      minSlice = srvd.Texture1DArray.FirstArraySlice;
      numSlices = srvd.Texture1DArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2D:
      minMip = srvd.Texture2D.MostDetailedMip;
      numMips = srvd.Texture2D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
      minMip = srvd.Texture2DArray.MostDetailedMip;
      numMips = srvd.Texture2DArray.MipLevels;
      minSlice = srvd.Texture2DArray.FirstArraySlice;
      numSlices = srvd.Texture2DArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS: break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
      minSlice = srvd.Texture2DMSArray.FirstArraySlice;
      numSlices = srvd.Texture2DMSArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE3D:
      minMip = srvd.Texture3D.MostDetailedMip;
      numMips = srvd.Texture3D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE:
      minMip = srvd.TextureCube.MostDetailedMip;
      numMips = srvd.TextureCube.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
      minMip = srvd.TextureCubeArray.MostDetailedMip;
      numMips = srvd.TextureCubeArray.MipLevels;
      minSlice = srvd.TextureCubeArray.First2DArrayFace;
      numSlices = srvd.TextureCubeArray.NumCubes * 6;
      break;
    case D3D11_SRV_DIMENSION_UNKNOWN:
    case D3D11_SRV_DIMENSION_BUFFER:
    case D3D11_SRV_DIMENSION_BUFFEREX: break;
  }

  SetMaxes(numMips, numSlices);
}

ResourceRange::ResourceRange(ID3D11UnorderedAccessView *uav)
{
  minMip = minSlice = 0;
  depthReadOnly = false;
  stencilReadOnly = false;

  if(uav == NULL)
  {
    resource = NULL;
    maxMip = allMip;
    maxSlice = allSlice;
    fullRange = true;
    return;
  }

#if ENABLED(RDOC_DEVEL)
  RDCASSERT(WrappedID3D11UnorderedAccessView1::IsAlloc(uav));
#endif

  ID3D11Resource *res = NULL;
  uav->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = allMip, numSlices = allSlice;

  D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
  uav->GetDesc(&desc);

  switch(desc.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      minMip = desc.Texture1D.MipSlice;
      numMips = 1;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      minMip = desc.Texture1DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture1DArray.FirstArraySlice;
      numSlices = desc.Texture1DArray.ArraySize;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      minMip = desc.Texture2D.MipSlice;
      numMips = 1;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      minMip = desc.Texture2DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture2DArray.FirstArraySlice;
      numSlices = desc.Texture2DArray.ArraySize;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      minMip = desc.Texture3D.MipSlice;
      numMips = 1;
      minSlice = desc.Texture3D.FirstWSlice;
      numSlices = desc.Texture3D.WSize;
      break;
    case D3D11_UAV_DIMENSION_UNKNOWN:
    case D3D11_UAV_DIMENSION_BUFFER: break;
  }

  SetMaxes(numMips, numSlices);
}

ResourceRange::ResourceRange(ID3D11RenderTargetView *rtv)
{
  minMip = minSlice = 0;
  depthReadOnly = false;
  stencilReadOnly = false;

  if(rtv == NULL)
  {
    resource = NULL;
    maxMip = allMip;
    maxSlice = allSlice;
    fullRange = true;
    return;
  }

#if ENABLED(RDOC_DEVEL)
  RDCASSERT(WrappedID3D11RenderTargetView1::IsAlloc(rtv));
#endif

  ID3D11Resource *res = NULL;
  rtv->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = allMip, numSlices = allSlice;

  D3D11_RENDER_TARGET_VIEW_DESC desc;
  rtv->GetDesc(&desc);

  switch(desc.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_TEXTURE1D:
      minMip = desc.Texture1D.MipSlice;
      numMips = 1;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
      minMip = desc.Texture1DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture1DArray.FirstArraySlice;
      numSlices = desc.Texture1DArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2D:
      minMip = desc.Texture2D.MipSlice;
      numMips = 1;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
      minMip = desc.Texture2DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture2DArray.FirstArraySlice;
      numSlices = desc.Texture2DArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS: break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
      minSlice = desc.Texture2DMSArray.FirstArraySlice;
      numSlices = desc.Texture2DMSArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE3D:
      minMip = desc.Texture3D.MipSlice;
      numMips = 1;
      minSlice = desc.Texture3D.FirstWSlice;
      numSlices = desc.Texture3D.WSize;
      break;
    case D3D11_RTV_DIMENSION_UNKNOWN:
    case D3D11_RTV_DIMENSION_BUFFER: break;
  }

  SetMaxes(numMips, numSlices);
}

ResourceRange::ResourceRange(ID3D11DepthStencilView *dsv)
{
  minMip = minSlice = 0;
  depthReadOnly = false;
  stencilReadOnly = false;

  if(dsv == NULL)
  {
    resource = NULL;
    maxMip = allMip;
    maxSlice = allSlice;
    fullRange = true;
    return;
  }

#if ENABLED(RDOC_DEVEL)
  RDCASSERT(WrappedID3D11DepthStencilView::IsAlloc(dsv));
#endif

  ID3D11Resource *res = NULL;
  dsv->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = allMip, numSlices = allSlice;

  D3D11_DEPTH_STENCIL_VIEW_DESC desc;
  dsv->GetDesc(&desc);

  if(desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
    depthReadOnly = true;
  if(desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
    stencilReadOnly = true;

  switch(desc.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_TEXTURE1D:
      minMip = desc.Texture1D.MipSlice;
      numMips = 1;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
      minMip = desc.Texture1DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture1DArray.FirstArraySlice;
      numSlices = desc.Texture1DArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2D:
      minMip = desc.Texture2D.MipSlice;
      numMips = 1;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      minMip = desc.Texture2DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture2DArray.FirstArraySlice;
      numSlices = desc.Texture2DArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS: break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      minSlice = desc.Texture2DMSArray.FirstArraySlice;
      numSlices = desc.Texture2DMSArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_UNKNOWN: break;
  }

  SetMaxes(numMips, numSlices);
}

ResourceRange::ResourceRange(ID3D11Buffer *res)
{
#if ENABLED(RDOC_DEVEL)
  RDCASSERT(!res || WrappedID3D11Buffer::IsAlloc(res));
#endif

  resource = res;
  minMip = minSlice = 0;
  maxMip = allMip;
  maxSlice = allSlice;
  fullRange = true;
  depthReadOnly = false;
  stencilReadOnly = false;
}

ResourceRange::ResourceRange(ID3D11Texture2D *res)
{
#if ENABLED(RDOC_DEVEL)
  RDCASSERT(!res || WrappedID3D11Texture2D1::IsAlloc(res));
#endif

  resource = res;
  minMip = minSlice = 0;
  maxMip = allMip;
  maxSlice = allSlice;
  fullRange = true;
  depthReadOnly = false;
  stencilReadOnly = false;
}

ResourceRange::ResourceRange(ID3D11Resource *res, UINT mip, UINT slice)
{
#if ENABLED(RDOC_DEVEL)
  RDCASSERT(!res || WrappedID3D11Texture1D::IsAlloc(res) || WrappedID3D11Texture2D1::IsAlloc(res) ||
            WrappedID3D11Texture3D1::IsAlloc(res) || WrappedID3D11Buffer::IsAlloc(res));
#endif

  resource = res;
  minMip = mip;
  maxMip = mip;
  minSlice = slice;
  maxSlice = slice;
  fullRange = false;
  depthReadOnly = false;
  stencilReadOnly = false;
}

D3D11InitParams::D3D11InitParams()
{
  DriverType = D3D_DRIVER_TYPE_UNKNOWN;
  Flags = 0;
  SDKVersion = D3D11_SDK_VERSION;
  NumFeatureLevels = 0;
  RDCEraseEl(FeatureLevels);
}

bool D3D11InitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // 0x0F -> 0x10 - serialised the number of subresources in resource initial states after
  // multiplying on sample count rather than before
  if(ver == 0x0F)
    return true;

  return false;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11InitParams &el)
{
  SERIALISE_MEMBER(DriverType);
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(SDKVersion);
  SERIALISE_MEMBER(NumFeatureLevels);
  SERIALISE_MEMBER(FeatureLevels);
}

INSTANTIATE_SERIALISE_TYPE(D3D11InitParams);

TextureType MakeTextureDim(D3D11_SRV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_SRV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D11_SRV_DIMENSION_BUFFER:
    case D3D11_SRV_DIMENSION_BUFFEREX: return TextureType::Buffer;
    case D3D11_SRV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D11_SRV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D11_SRV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
    case D3D11_SRV_DIMENSION_TEXTURECUBE: return TextureType::TextureCube;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: return TextureType::TextureCubeArray;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D11_RTV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_RTV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D11_RTV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D11_RTV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D11_RTV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D11_RTV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D11_DSV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_DSV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D11_DSV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D11_DSV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D11_UAV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_UAV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D11_UAV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D11_UAV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D11_UAV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D11_UAV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
  }

  return TextureType::Unknown;
}

AddressMode MakeAddressMode(D3D11_TEXTURE_ADDRESS_MODE addr)
{
  switch(addr)
  {
    case D3D11_TEXTURE_ADDRESS_WRAP: return AddressMode::Wrap;
    case D3D11_TEXTURE_ADDRESS_MIRROR: return AddressMode::Mirror;
    case D3D11_TEXTURE_ADDRESS_CLAMP: return AddressMode::ClampEdge;
    case D3D11_TEXTURE_ADDRESS_BORDER: return AddressMode::ClampBorder;
    case D3D11_TEXTURE_ADDRESS_MIRROR_ONCE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

CompareFunction MakeCompareFunc(D3D11_COMPARISON_FUNC func)
{
  switch(func)
  {
    case D3D11_COMPARISON_NEVER: return CompareFunction::Never;
    case D3D11_COMPARISON_LESS: return CompareFunction::Less;
    case D3D11_COMPARISON_EQUAL: return CompareFunction::Equal;
    case D3D11_COMPARISON_LESS_EQUAL: return CompareFunction::LessEqual;
    case D3D11_COMPARISON_GREATER: return CompareFunction::Greater;
    case D3D11_COMPARISON_NOT_EQUAL: return CompareFunction::NotEqual;
    case D3D11_COMPARISON_GREATER_EQUAL: return CompareFunction::GreaterEqual;
    case D3D11_COMPARISON_ALWAYS: return CompareFunction::AlwaysTrue;
    default: break;
  }

  return CompareFunction::AlwaysTrue;
}

TextureFilter MakeFilter(D3D11_FILTER filter)
{
  TextureFilter ret;

  ret.filter = FilterFunction::Normal;

  if(filter >= D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT &&
     filter < D3D11_FILTER_COMPARISON_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Comparison;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D11_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D11_FILTER_MINIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D11_FILTER_MINIMUM_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Minimum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D11_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D11_FILTER_MAXIMUM_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Maximum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D11_FILTER(filter & 0x7f);
  }

  if(filter == D3D11_FILTER_ANISOTROPIC)
  {
    ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
  }
  else
  {
    switch(filter)
    {
      case D3D11_FILTER_MIN_MAG_MIP_POINT:
        ret.minify = ret.magnify = ret.mip = FilterMode::Point;
        break;
      case D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR:
        ret.minify = ret.magnify = FilterMode::Point;
        ret.mip = FilterMode::Linear;
        break;
      case D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
        ret.minify = FilterMode::Point;
        ret.magnify = FilterMode::Linear;
        ret.mip = FilterMode::Point;
        break;
      case D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR:
        ret.minify = FilterMode::Point;
        ret.magnify = ret.mip = FilterMode::Linear;
        break;
      case D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT:
        ret.minify = FilterMode::Linear;
        ret.magnify = ret.mip = FilterMode::Point;
        break;
      case D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
        ret.minify = FilterMode::Linear;
        ret.magnify = FilterMode::Point;
        ret.mip = FilterMode::Linear;
        break;
      case D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT:
        ret.minify = ret.magnify = FilterMode::Linear;
        ret.mip = FilterMode::Point;
        break;
      case D3D11_FILTER_MIN_MAG_MIP_LINEAR:
        ret.minify = ret.magnify = ret.mip = FilterMode::Linear;
        break;
    }
  }

  return ret;
}

LogicOperation MakeLogicOp(D3D11_LOGIC_OP op)
{
  switch(op)
  {
    case D3D11_LOGIC_OP_CLEAR: return LogicOperation::Clear;
    case D3D11_LOGIC_OP_AND: return LogicOperation::And;
    case D3D11_LOGIC_OP_AND_REVERSE: return LogicOperation::AndReverse;
    case D3D11_LOGIC_OP_COPY: return LogicOperation::Copy;
    case D3D11_LOGIC_OP_AND_INVERTED: return LogicOperation::AndInverted;
    case D3D11_LOGIC_OP_NOOP: return LogicOperation::NoOp;
    case D3D11_LOGIC_OP_XOR: return LogicOperation::Xor;
    case D3D11_LOGIC_OP_OR: return LogicOperation::Or;
    case D3D11_LOGIC_OP_NOR: return LogicOperation::Nor;
    case D3D11_LOGIC_OP_EQUIV: return LogicOperation::Equivalent;
    case D3D11_LOGIC_OP_INVERT: return LogicOperation::Invert;
    case D3D11_LOGIC_OP_OR_REVERSE: return LogicOperation::OrReverse;
    case D3D11_LOGIC_OP_COPY_INVERTED: return LogicOperation::CopyInverted;
    case D3D11_LOGIC_OP_OR_INVERTED: return LogicOperation::OrInverted;
    case D3D11_LOGIC_OP_NAND: return LogicOperation::Nand;
    case D3D11_LOGIC_OP_SET: return LogicOperation::Set;
    default: break;
  }

  return LogicOperation::NoOp;
}

BlendMultiplier MakeBlendMultiplier(D3D11_BLEND blend, bool alpha)
{
  switch(blend)
  {
    case D3D11_BLEND_ZERO: return BlendMultiplier::Zero;
    case D3D11_BLEND_ONE: return BlendMultiplier::One;
    case D3D11_BLEND_SRC_COLOR: return BlendMultiplier::SrcCol;
    case D3D11_BLEND_INV_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case D3D11_BLEND_DEST_COLOR: return BlendMultiplier::DstCol;
    case D3D11_BLEND_INV_DEST_COLOR: return BlendMultiplier::InvDstCol;
    case D3D11_BLEND_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case D3D11_BLEND_INV_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case D3D11_BLEND_DEST_ALPHA: return BlendMultiplier::DstAlpha;
    case D3D11_BLEND_INV_DEST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case D3D11_BLEND_BLEND_FACTOR:
      return alpha ? BlendMultiplier::FactorAlpha : BlendMultiplier::FactorRGB;
    case D3D11_BLEND_INV_BLEND_FACTOR:
      return alpha ? BlendMultiplier::InvFactorAlpha : BlendMultiplier::InvFactorRGB;
    case D3D11_BLEND_SRC_ALPHA_SAT: return BlendMultiplier::SrcAlphaSat;
    case D3D11_BLEND_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case D3D11_BLEND_INV_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case D3D11_BLEND_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case D3D11_BLEND_INV_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(D3D11_BLEND_OP op)
{
  switch(op)
  {
    case D3D11_BLEND_OP_ADD: return BlendOperation::Add;
    case D3D11_BLEND_OP_SUBTRACT: return BlendOperation::Subtract;
    case D3D11_BLEND_OP_REV_SUBTRACT: return BlendOperation::ReversedSubtract;
    case D3D11_BLEND_OP_MIN: return BlendOperation::Minimum;
    case D3D11_BLEND_OP_MAX: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

StencilOperation MakeStencilOp(D3D11_STENCIL_OP op)
{
  switch(op)
  {
    case D3D11_STENCIL_OP_KEEP: return StencilOperation::Keep;
    case D3D11_STENCIL_OP_ZERO: return StencilOperation::Zero;
    case D3D11_STENCIL_OP_REPLACE: return StencilOperation::Replace;
    case D3D11_STENCIL_OP_INCR_SAT: return StencilOperation::IncSat;
    case D3D11_STENCIL_OP_DECR_SAT: return StencilOperation::DecSat;
    case D3D11_STENCIL_OP_INVERT: return StencilOperation::Invert;
    case D3D11_STENCIL_OP_INCR: return StencilOperation::IncWrap;
    case D3D11_STENCIL_OP_DECR: return StencilOperation::DecWrap;
    default: break;
  }

  return StencilOperation::Keep;
}
