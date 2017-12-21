/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "d3d12_common.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

D3D12MarkerRegion::D3D12MarkerRegion(ID3D12GraphicsCommandList *l, const std::string &marker)
{
  list = l;
  queue = NULL;

  D3D12MarkerRegion::Begin(list, marker);
}

D3D12MarkerRegion::D3D12MarkerRegion(ID3D12CommandQueue *q, const std::string &marker)
{
  list = NULL;
  queue = q;

  D3D12MarkerRegion::Begin(queue, marker);
}

D3D12MarkerRegion::~D3D12MarkerRegion()
{
  if(list)
    D3D12MarkerRegion::End(list);
  if(queue)
    D3D12MarkerRegion::End(queue);
}

void D3D12MarkerRegion::Begin(ID3D12GraphicsCommandList *list, const std::string &marker)
{
  if(list)
  {
    std::wstring text = StringFormat::UTF82Wide(marker);
    list->BeginEvent(0, text.c_str(), (UINT)text.size());
  }
}

void D3D12MarkerRegion::Begin(ID3D12CommandQueue *queue, const std::string &marker)
{
  if(queue)
  {
    std::wstring text = StringFormat::UTF82Wide(marker);
    queue->BeginEvent(0, text.c_str(), (UINT)text.size());
  }
}

void D3D12MarkerRegion::Set(ID3D12GraphicsCommandList *list, const std::string &marker)
{
  if(list)
  {
    std::wstring text = StringFormat::UTF82Wide(marker);
    list->SetMarker(0, text.c_str(), (UINT)text.size());
  }
}

void D3D12MarkerRegion::Set(ID3D12CommandQueue *queue, const std::string &marker)
{
  if(queue)
  {
    std::wstring text = StringFormat::UTF82Wide(marker);
    queue->SetMarker(0, text.c_str(), (UINT)text.size());
  }
}

void D3D12MarkerRegion::End(ID3D12GraphicsCommandList *list)
{
  list->EndEvent();
}

void D3D12MarkerRegion::End(ID3D12CommandQueue *queue)
{
  queue->EndEvent();
}

D3D12InitParams::D3D12InitParams()
{
  MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;
}

bool D3D12InitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // we can check other older versions we support here.

  return false;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12InitParams &el)
{
  SERIALISE_MEMBER(MinimumFeatureLevel);
}

INSTANTIATE_SERIALISE_TYPE(D3D12InitParams);

TextureType MakeTextureDim(D3D12_SRV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_SRV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D12_SRV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_SRV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D12_SRV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
    case D3D12_SRV_DIMENSION_TEXTURECUBE: return TextureType::TextureCube;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: return TextureType::TextureCubeArray;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D12_RTV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_RTV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_RTV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D12_RTV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_RTV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D12_RTV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D12_DSV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_DSV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_DSV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_DSV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D12_UAV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_UAV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_UAV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D12_UAV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_UAV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_UAV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
  }

  return TextureType::Unknown;
}

AddressMode MakeAddressMode(D3D12_TEXTURE_ADDRESS_MODE addr)
{
  switch(addr)
  {
    case D3D12_TEXTURE_ADDRESS_MODE_WRAP: return AddressMode::Wrap;
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR: return AddressMode::Mirror;
    case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: return AddressMode::ClampEdge;
    case D3D12_TEXTURE_ADDRESS_MODE_BORDER: return AddressMode::ClampBorder;
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

CompareFunction MakeCompareFunc(D3D12_COMPARISON_FUNC func)
{
  switch(func)
  {
    case D3D12_COMPARISON_FUNC_NEVER: return CompareFunction::Never;
    case D3D12_COMPARISON_FUNC_LESS: return CompareFunction::Less;
    case D3D12_COMPARISON_FUNC_EQUAL: return CompareFunction::Equal;
    case D3D12_COMPARISON_FUNC_LESS_EQUAL: return CompareFunction::LessEqual;
    case D3D12_COMPARISON_FUNC_GREATER: return CompareFunction::Greater;
    case D3D12_COMPARISON_FUNC_NOT_EQUAL: return CompareFunction::NotEqual;
    case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return CompareFunction::GreaterEqual;
    case D3D12_COMPARISON_FUNC_ALWAYS: return CompareFunction::AlwaysTrue;
    default: break;
  }

  return CompareFunction::AlwaysTrue;
}

TextureFilter MakeFilter(D3D12_FILTER filter)
{
  TextureFilter ret;

  ret.filter = FilterFunction::Normal;

  if(filter >= D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT &&
     filter < D3D12_FILTER_COMPARISON_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Comparison;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D12_FILTER_MINIMUM_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Minimum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D12_FILTER_MAXIMUM_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Maximum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }

  if(filter == D3D12_FILTER_ANISOTROPIC)
  {
    ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
  }
  else
  {
    switch(filter)
    {
      case D3D12_FILTER_MIN_MAG_MIP_POINT:
        ret.minify = ret.magnify = ret.mip = FilterMode::Point;
        break;
      case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:
        ret.minify = ret.magnify = FilterMode::Point;
        ret.mip = FilterMode::Linear;
        break;
      case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
        ret.minify = FilterMode::Point;
        ret.magnify = FilterMode::Linear;
        ret.mip = FilterMode::Point;
        break;
      case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:
        ret.minify = FilterMode::Point;
        ret.magnify = ret.mip = FilterMode::Linear;
        break;
      case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:
        ret.minify = FilterMode::Linear;
        ret.magnify = ret.mip = FilterMode::Point;
        break;
      case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
        ret.minify = FilterMode::Linear;
        ret.magnify = FilterMode::Point;
        ret.mip = FilterMode::Linear;
        break;
      case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:
        ret.minify = ret.magnify = FilterMode::Linear;
        ret.mip = FilterMode::Point;
        break;
      case D3D12_FILTER_MIN_MAG_MIP_LINEAR:
        ret.minify = ret.magnify = ret.mip = FilterMode::Linear;
        break;
    }
  }

  return ret;
}

D3DBufferViewFlags MakeBufferFlags(D3D12_BUFFER_SRV_FLAGS flags)
{
  D3DBufferViewFlags ret = D3DBufferViewFlags::NoFlags;

  if(flags & D3D12_BUFFER_SRV_FLAG_RAW)
    ret |= D3DBufferViewFlags::Raw;

  return ret;
}

D3DBufferViewFlags MakeBufferFlags(D3D12_BUFFER_UAV_FLAGS flags)
{
  D3DBufferViewFlags ret = D3DBufferViewFlags::NoFlags;

  if(flags & D3D12_BUFFER_UAV_FLAG_RAW)
    ret |= D3DBufferViewFlags::Raw;

  return ret;
}

LogicOperation MakeLogicOp(D3D12_LOGIC_OP op)
{
  switch(op)
  {
    case D3D12_LOGIC_OP_CLEAR: return LogicOperation::Clear;
    case D3D12_LOGIC_OP_AND: return LogicOperation::And;
    case D3D12_LOGIC_OP_AND_REVERSE: return LogicOperation::AndReverse;
    case D3D12_LOGIC_OP_COPY: return LogicOperation::Copy;
    case D3D12_LOGIC_OP_AND_INVERTED: return LogicOperation::AndInverted;
    case D3D12_LOGIC_OP_NOOP: return LogicOperation::NoOp;
    case D3D12_LOGIC_OP_XOR: return LogicOperation::Xor;
    case D3D12_LOGIC_OP_OR: return LogicOperation::Or;
    case D3D12_LOGIC_OP_NOR: return LogicOperation::Nor;
    case D3D12_LOGIC_OP_EQUIV: return LogicOperation::Equivalent;
    case D3D12_LOGIC_OP_INVERT: return LogicOperation::Invert;
    case D3D12_LOGIC_OP_OR_REVERSE: return LogicOperation::OrReverse;
    case D3D12_LOGIC_OP_COPY_INVERTED: return LogicOperation::CopyInverted;
    case D3D12_LOGIC_OP_OR_INVERTED: return LogicOperation::OrInverted;
    case D3D12_LOGIC_OP_NAND: return LogicOperation::Nand;
    case D3D12_LOGIC_OP_SET: return LogicOperation::Set;
    default: break;
  }

  return LogicOperation::NoOp;
}

BlendMultiplier MakeBlendMultiplier(D3D12_BLEND blend, bool alpha)
{
  switch(blend)
  {
    case D3D12_BLEND_ZERO: return BlendMultiplier::Zero;
    case D3D12_BLEND_ONE: return BlendMultiplier::One;
    case D3D12_BLEND_SRC_COLOR: return BlendMultiplier::SrcCol;
    case D3D12_BLEND_INV_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case D3D12_BLEND_DEST_COLOR: return BlendMultiplier::DstCol;
    case D3D12_BLEND_INV_DEST_COLOR: return BlendMultiplier::InvDstCol;
    case D3D12_BLEND_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case D3D12_BLEND_INV_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case D3D12_BLEND_DEST_ALPHA: return BlendMultiplier::DstAlpha;
    case D3D12_BLEND_INV_DEST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case D3D12_BLEND_BLEND_FACTOR:
      return alpha ? BlendMultiplier::FactorAlpha : BlendMultiplier::FactorRGB;
    case D3D12_BLEND_INV_BLEND_FACTOR:
      return alpha ? BlendMultiplier::InvFactorAlpha : BlendMultiplier::InvFactorRGB;
    case D3D12_BLEND_SRC_ALPHA_SAT: return BlendMultiplier::SrcAlphaSat;
    case D3D12_BLEND_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case D3D12_BLEND_INV_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case D3D12_BLEND_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case D3D12_BLEND_INV_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(D3D12_BLEND_OP op)
{
  switch(op)
  {
    case D3D12_BLEND_OP_ADD: return BlendOperation::Add;
    case D3D12_BLEND_OP_SUBTRACT: return BlendOperation::Subtract;
    case D3D12_BLEND_OP_REV_SUBTRACT: return BlendOperation::ReversedSubtract;
    case D3D12_BLEND_OP_MIN: return BlendOperation::Minimum;
    case D3D12_BLEND_OP_MAX: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

StencilOperation MakeStencilOp(D3D12_STENCIL_OP op)
{
  switch(op)
  {
    case D3D12_STENCIL_OP_KEEP: return StencilOperation::Keep;
    case D3D12_STENCIL_OP_ZERO: return StencilOperation::Zero;
    case D3D12_STENCIL_OP_REPLACE: return StencilOperation::Replace;
    case D3D12_STENCIL_OP_INCR_SAT: return StencilOperation::IncSat;
    case D3D12_STENCIL_OP_DECR_SAT: return StencilOperation::DecSat;
    case D3D12_STENCIL_OP_INVERT: return StencilOperation::Invert;
    case D3D12_STENCIL_OP_INCR: return StencilOperation::IncWrap;
    case D3D12_STENCIL_OP_DECR: return StencilOperation::DecWrap;
    default: break;
  }

  return StencilOperation::Keep;
}

UINT GetResourceNumMipLevels(const D3D12_RESOURCE_DESC *desc)
{
  switch(desc->Dimension)
  {
    default:
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
      RDCERR("Unexpected resource dimension! %d", desc->Dimension);
      break;
    case D3D12_RESOURCE_DIMENSION_BUFFER: return 1;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT count = 1;
      while(w > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
      }
      return count;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT h = RDCMAX(1U, desc->Height);
      UINT count = 1;
      while(w > 1 || h > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
        h = RDCMAX(1U, h >> 1U);
      }
      return count;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT h = RDCMAX(1U, desc->Height);
      UINT d = RDCMAX(1U, UINT(desc->DepthOrArraySize));
      UINT count = 1;
      while(w > 1 || h > 1 || d > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
        h = RDCMAX(1U, h >> 1U);
        d = RDCMAX(1U, d >> 1U);
      }
      return count;
    }
  }

  return 1;
}

UINT GetNumSubresources(ID3D12Device *dev, const D3D12_RESOURCE_DESC *desc)
{
  D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
  formatInfo.Format = desc->Format;
  dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

  UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

  switch(desc->Dimension)
  {
    default:
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
      RDCERR("Unexpected resource dimension! %d", desc->Dimension);
      break;
    case D3D12_RESOURCE_DIMENSION_BUFFER: return planes;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
      return RDCMAX((UINT16)1, desc->DepthOrArraySize) * GetResourceNumMipLevels(desc) * planes;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: return GetResourceNumMipLevels(desc) * planes;
  }

  return 1;
}

ShaderStageMask ConvertVisibility(D3D12_SHADER_VISIBILITY ShaderVisibility)
{
  switch(ShaderVisibility)
  {
    case D3D12_SHADER_VISIBILITY_ALL: return ShaderStageMask::All;
    case D3D12_SHADER_VISIBILITY_VERTEX: return ShaderStageMask::Vertex;
    case D3D12_SHADER_VISIBILITY_HULL: return ShaderStageMask::Hull;
    case D3D12_SHADER_VISIBILITY_DOMAIN: return ShaderStageMask::Domain;
    case D3D12_SHADER_VISIBILITY_GEOMETRY: return ShaderStageMask::Geometry;
    case D3D12_SHADER_VISIBILITY_PIXEL: return ShaderStageMask::Pixel;
  }

  return ShaderStageMask::Vertex;
}
