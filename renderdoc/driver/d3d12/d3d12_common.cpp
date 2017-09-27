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

TextureDim MakeTextureDim(D3D12_SRV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D12_SRV_DIMENSION_BUFFER: return TextureDim::Buffer;
    case D3D12_SRV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D12_SRV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS: return TextureDim::Texture2DMS;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: return TextureDim::Texture2DMSArray;
    case D3D12_SRV_DIMENSION_TEXTURE3D: return TextureDim::Texture3D;
    case D3D12_SRV_DIMENSION_TEXTURECUBE: return TextureDim::TextureCube;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: return TextureDim::TextureCubeArray;
  }

  return TextureDim::Unknown;
}

TextureDim MakeTextureDim(D3D12_RTV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_RTV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D12_RTV_DIMENSION_BUFFER: return TextureDim::Buffer;
    case D3D12_RTV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D12_RTV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS: return TextureDim::Texture2DMS;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: return TextureDim::Texture2DMSArray;
    case D3D12_RTV_DIMENSION_TEXTURE3D: return TextureDim::Texture3D;
  }

  return TextureDim::Unknown;
}

TextureDim MakeTextureDim(D3D12_DSV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_DSV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D12_DSV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D12_DSV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS: return TextureDim::Texture2DMS;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: return TextureDim::Texture2DMSArray;
  }

  return TextureDim::Unknown;
}

TextureDim MakeTextureDim(D3D12_UAV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_UAV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D12_UAV_DIMENSION_BUFFER: return TextureDim::Buffer;
    case D3D12_UAV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D12_UAV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D12_UAV_DIMENSION_TEXTURE3D: return TextureDim::Texture3D;
  }

  return TextureDim::Unknown;
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

CompareFunc MakeCompareFunc(D3D12_COMPARISON_FUNC func)
{
  switch(func)
  {
    case D3D12_COMPARISON_FUNC_NEVER: return CompareFunc::Never;
    case D3D12_COMPARISON_FUNC_LESS: return CompareFunc::Less;
    case D3D12_COMPARISON_FUNC_EQUAL: return CompareFunc::Equal;
    case D3D12_COMPARISON_FUNC_LESS_EQUAL: return CompareFunc::LessEqual;
    case D3D12_COMPARISON_FUNC_GREATER: return CompareFunc::Greater;
    case D3D12_COMPARISON_FUNC_NOT_EQUAL: return CompareFunc::NotEqual;
    case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return CompareFunc::GreaterEqual;
    case D3D12_COMPARISON_FUNC_ALWAYS: return CompareFunc::AlwaysTrue;
    default: break;
  }

  return CompareFunc::AlwaysTrue;
}

TextureFilter MakeFilter(D3D12_FILTER filter)
{
  TextureFilter ret;

  ret.func = FilterFunc::Normal;

  if(filter >= D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT &&
     filter < D3D12_FILTER_COMPARISON_ANISOTROPIC)
  {
    ret.func = FilterFunc::Comparison;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D12_FILTER_MINIMUM_ANISOTROPIC)
  {
    ret.func = FilterFunc::Minimum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D12_FILTER_MAXIMUM_ANISOTROPIC)
  {
    ret.func = FilterFunc::Maximum;
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

LogicOp MakeLogicOp(D3D12_LOGIC_OP op)
{
  switch(op)
  {
    case D3D12_LOGIC_OP_CLEAR: return LogicOp::Clear;
    case D3D12_LOGIC_OP_AND: return LogicOp::And;
    case D3D12_LOGIC_OP_AND_REVERSE: return LogicOp::AndReverse;
    case D3D12_LOGIC_OP_COPY: return LogicOp::Copy;
    case D3D12_LOGIC_OP_AND_INVERTED: return LogicOp::AndInverted;
    case D3D12_LOGIC_OP_NOOP: return LogicOp::NoOp;
    case D3D12_LOGIC_OP_XOR: return LogicOp::Xor;
    case D3D12_LOGIC_OP_OR: return LogicOp::Or;
    case D3D12_LOGIC_OP_NOR: return LogicOp::Nor;
    case D3D12_LOGIC_OP_EQUIV: return LogicOp::Equivalent;
    case D3D12_LOGIC_OP_INVERT: return LogicOp::Invert;
    case D3D12_LOGIC_OP_OR_REVERSE: return LogicOp::OrReverse;
    case D3D12_LOGIC_OP_COPY_INVERTED: return LogicOp::CopyInverted;
    case D3D12_LOGIC_OP_OR_INVERTED: return LogicOp::OrInverted;
    case D3D12_LOGIC_OP_NAND: return LogicOp::Nand;
    case D3D12_LOGIC_OP_SET: return LogicOp::Set;
    default: break;
  }

  return LogicOp::NoOp;
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

BlendOp MakeBlendOp(D3D12_BLEND_OP op)
{
  switch(op)
  {
    case D3D12_BLEND_OP_ADD: return BlendOp::Add;
    case D3D12_BLEND_OP_SUBTRACT: return BlendOp::Subtract;
    case D3D12_BLEND_OP_REV_SUBTRACT: return BlendOp::ReversedSubtract;
    case D3D12_BLEND_OP_MIN: return BlendOp::Minimum;
    case D3D12_BLEND_OP_MAX: return BlendOp::Maximum;
    default: break;
  }

  return BlendOp::Add;
}

StencilOp MakeStencilOp(D3D12_STENCIL_OP op)
{
  switch(op)
  {
    case D3D12_STENCIL_OP_KEEP: return StencilOp::Keep;
    case D3D12_STENCIL_OP_ZERO: return StencilOp::Zero;
    case D3D12_STENCIL_OP_REPLACE: return StencilOp::Replace;
    case D3D12_STENCIL_OP_INCR_SAT: return StencilOp::IncSat;
    case D3D12_STENCIL_OP_DECR_SAT: return StencilOp::DecSat;
    case D3D12_STENCIL_OP_INVERT: return StencilOp::Invert;
    case D3D12_STENCIL_OP_INCR: return StencilOp::IncWrap;
    case D3D12_STENCIL_OP_DECR: return StencilOp::DecWrap;
    default: break;
  }

  return StencilOp::Keep;
}

enum D3D12ResourceBarrierSubresource
{
  D3D12AllSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
};

template <>
std::string DoStringise(const D3D12ResourceBarrierSubresource &el)
{
  if(el == D3D12AllSubresources)
    return "All Subresources";

  return ToStr(uint32_t(el));
}

enum D3D12ComponentMapping
{
};

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

template <>
std::string DoStringise(const D3D12ComponentMapping &el)
{
  std::string ret;

  // value should always be <= 5, see D3D12_SHADER_COMPONENT_MAPPING
  const char mapping[] = {'R', 'G', 'B', 'A', '0', '1', '?', '!'};

  uint32_t swizzle = (uint32_t)el;

  for(int i = 0; i < 4; i++)
    ret += mapping[D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(i, swizzle)];

  return ret;
}

#define SerialiseObject(type, name, obj)                                                        \
  {                                                                                             \
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();                           \
    ResourceId id;                                                                              \
    if(m_Mode >= WRITING)                                                                       \
      id = GetResID(obj);                                                                       \
    Serialise(name, id);                                                                        \
    if(m_Mode < WRITING)                                                                        \
      obj = (id == ResourceId() || !rm->HasLiveResource(id)) ? NULL                             \
                                                             : Unwrap(rm->GetLiveAs<type>(id)); \
  }

#define SerialiseWrappedObject(type, name, obj)                                                \
  {                                                                                            \
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();                          \
    ResourceId id;                                                                             \
    if(m_Mode >= WRITING)                                                                      \
      id = GetResID(obj);                                                                      \
    Serialise(name, id);                                                                       \
    if(m_Mode < WRITING)                                                                       \
      obj = (id == ResourceId() || !rm->HasLiveResource(id)) ? NULL : rm->GetLiveAs<type>(id); \
  }

template <>
void Serialiser::Serialise(const char *name, D3D12Descriptor &el)
{
  ScopedContext scope(this, name, "D3D12Descriptor", 0, true);

  D3D12Descriptor::DescriptorType type = el.GetType();
  Serialise("type", type);

  // we serialise the heap by hand because we want to keep it wrapped
  {
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

    PortableHandle handle;

    if(m_Mode >= WRITING)
      handle =
          PortableHandle(el.samp.heap ? el.samp.heap->GetResourceID() : ResourceId(), el.samp.idx);

    Serialise("handle", handle);

    if(m_Mode < WRITING)
    {
      el.samp.heap = (handle.heap == ResourceId() || !rm->HasLiveResource(handle.heap))
                         ? NULL
                         : rm->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap);
      el.samp.idx = handle.index;
    }
  }

  // for sampler types, this will be overwritten when serialising the sampler descriptor
  if(m_Mode < WRITING)
    el.nonsamp.type = type;

  switch(type)
  {
    case D3D12Descriptor::TypeSampler:
    {
      Serialise("Descriptor", el.samp.desc);
      RDCASSERTEQUAL(el.GetType(), D3D12Descriptor::TypeSampler);
      break;
    }
    case D3D12Descriptor::TypeCBV:
    {
      Serialise("Descriptor", el.nonsamp.cbv);
      break;
    }
    case D3D12Descriptor::TypeSRV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      Serialise("Descriptor", el.nonsamp.srv);
      break;
    }
    case D3D12Descriptor::TypeRTV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      Serialise("Descriptor", el.nonsamp.rtv);
      break;
    }
    case D3D12Descriptor::TypeDSV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      Serialise("Descriptor", el.nonsamp.dsv);
      break;
    }
    case D3D12Descriptor::TypeUAV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      SerialiseWrappedObject(ID3D12Resource, "CounterResource", el.nonsamp.uav.counterResource);

      // special case because of extra resource and squeezed descriptor
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = el.nonsamp.uav.desc.AsDesc();
      Serialise("Descriptor", desc);
      el.nonsamp.uav.desc.Init(desc);
      break;
    }
    case D3D12Descriptor::TypeUndefined:
    {
      el.nonsamp.type = type;
      break;
    }
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RESOURCE_DESC", 0, true);

  Serialise("Dimension", el.Dimension);
  Serialise("Alignment", el.Alignment);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("DepthOrArraySize", el.DepthOrArraySize);
  Serialise("MipLevels", el.MipLevels);
  Serialise("Format", el.Format);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("Layout", el.Layout);
  Serialise("Flags", el.Flags);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_QUEUE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_COMMAND_QUEUE_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("Priority", el.Priority);
  Serialise("Flags", el.Flags);
  Serialise("NodeMask", el.NodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_BYTECODE &el)
{
  ScopedContext scope(this, name, "D3D12_SHADER_BYTECODE", 0, true);

  uint64_t dataSize = el.BytecodeLength;
  Serialise("BytecodeLength", dataSize);
  size_t sz = (size_t)dataSize;
  if(m_Mode == READING)
  {
    el.pShaderBytecode = NULL;
    el.BytecodeLength = sz;
  }

  if(dataSize > 0)
    SerialiseBuffer("pShaderBytecode", (byte *&)el.pShaderBytecode, sz);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SO_DECLARATION_ENTRY &el)
{
  ScopedContext scope(this, name, "D3D12_SO_DECLARATION_ENTRY", 0, true);

  Serialise("Stream", el.Stream);

  {
    string s = "";
    if(m_Mode == WRITING && el.SemanticName)
      s = el.SemanticName;

    Serialise("SemanticName", s);

    if(m_Mode == READING)
    {
      m_StringDB.insert(s);
      el.SemanticName = m_StringDB.find(s)->c_str();
    }
  }

  Serialise("SemanticIndex", el.SemanticIndex);
  Serialise("StartComponent", el.StartComponent);
  Serialise("ComponentCount", el.ComponentCount);
  Serialise("OutputSlot", el.OutputSlot);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_STREAM_OUTPUT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_STREAM_OUTPUT_DESC", 0, true);

  if(m_Mode == READING)
  {
    el.pSODeclaration = NULL;
    el.pBufferStrides = NULL;
  }

  SerialiseComplexArray("pSODeclaration", (D3D12_SO_DECLARATION_ENTRY *&)el.pSODeclaration,
                        el.NumEntries);
  SerialisePODArray("pBufferStrides", (UINT *&)el.pBufferStrides, el.NumStrides);
  Serialise("RasterizedStream", el.RasterizedStream);
}

template <>
void Serialiser::Deserialise(const D3D12_STREAM_OUTPUT_DESC *const el) const
{
  if(m_Mode == READING)
  {
    delete[] el->pSODeclaration;
    delete[] el->pBufferStrides;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RENDER_TARGET_BLEND_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RENDER_TARGET_BLEND_DESC", 0, true);

  Serialise("BlendEnable", el.BlendEnable);
  Serialise("LogicOpEnable", el.LogicOpEnable);
  Serialise("SrcBlend", el.SrcBlend);
  Serialise("DestBlend", el.DestBlend);
  Serialise("BlendOp", el.BlendOp);
  Serialise("SrcBlendAlpha", el.SrcBlendAlpha);
  Serialise("DestBlendAlpha", el.DestBlendAlpha);
  Serialise("BlendOpAlpha", el.BlendOpAlpha);
  Serialise("LogicOp", el.LogicOp);
  Serialise("RenderTargetWriteMask", el.RenderTargetWriteMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_BLEND_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_BLEND_DESC", 0, true);

  Serialise("AlphaToCoverageEnable", el.AlphaToCoverageEnable);
  Serialise("IndependentBlendEnable", el.IndependentBlendEnable);
  for(int i = 0; i < 8; i++)
  {
    ScopedContext targetscope(this, name, "D3D12_RENDER_TARGET_BLEND_DESC", 0, true);

    bool enable = el.RenderTarget[i].BlendEnable == TRUE;
    Serialise("BlendEnable", enable);
    el.RenderTarget[i].BlendEnable = enable;

    enable = el.RenderTarget[i].LogicOpEnable == TRUE;
    Serialise("LogicOpEnable", enable);
    el.RenderTarget[i].LogicOpEnable = enable;

    Serialise("SrcBlend", el.RenderTarget[i].SrcBlend);
    Serialise("DestBlend", el.RenderTarget[i].DestBlend);
    Serialise("BlendOp", el.RenderTarget[i].BlendOp);
    Serialise("SrcBlendAlpha", el.RenderTarget[i].SrcBlendAlpha);
    Serialise("DestBlendAlpha", el.RenderTarget[i].DestBlendAlpha);
    Serialise("BlendOpAlpha", el.RenderTarget[i].BlendOpAlpha);
    Serialise("LogicOp", el.RenderTarget[i].LogicOp);
    Serialise("RenderTargetWriteMask", el.RenderTarget[i].RenderTargetWriteMask);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RASTERIZER_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RASTERIZER_DESC", 0, true);

  Serialise("FillMode", el.FillMode);
  Serialise("CullMode", el.CullMode);
  Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
  Serialise("DepthBias", el.DepthBias);
  Serialise("DepthBiasClamp", el.DepthBiasClamp);
  Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
  Serialise("DepthClipEnable", el.DepthClipEnable);
  Serialise("MultisampleEnable", el.MultisampleEnable);
  Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
  Serialise("ForcedSampleCount", el.ForcedSampleCount);
  Serialise("ConservativeRaster", el.ConservativeRaster);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCILOP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DEPTH_STENCILOP_DESC", 0, true);

  Serialise("StencilFailOp", el.StencilFailOp);
  Serialise("StencilDepthFailOp", el.StencilDepthFailOp);
  Serialise("StencilPassOp", el.StencilPassOp);
  Serialise("StencilFunc", el.StencilFunc);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCIL_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DEPTH_STENCIL_DESC", 0, true);

  Serialise("DepthEnable", el.DepthEnable);
  Serialise("DepthWriteMask", el.DepthWriteMask);
  Serialise("DepthFunc", el.DepthFunc);
  Serialise("StencilEnable", el.StencilEnable);
  Serialise("StencilReadMask", el.StencilReadMask);
  Serialise("StencilWriteMask", el.StencilWriteMask);
  Serialise("FrontFace", el.FrontFace);
  Serialise("BackFace", el.BackFace);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INPUT_ELEMENT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_INPUT_ELEMENT_DESC", 0, true);

  {
    string s = "";
    if(m_Mode == WRITING && el.SemanticName)
      s = el.SemanticName;

    Serialise("SemanticName", s);

    if(m_Mode == READING)
    {
      m_StringDB.insert(s);
      el.SemanticName = m_StringDB.find(s)->c_str();
    }
  }

  Serialise("SemanticIndex", el.SemanticIndex);
  Serialise("Format", el.Format);
  Serialise("InputSlot", el.InputSlot);
  Serialise("AlignedByteOffset", el.AlignedByteOffset);
  Serialise("InputSlotClass", el.InputSlotClass);
  Serialise("InstanceDataStepRate", el.InstanceDataStepRate);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INPUT_LAYOUT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_INPUT_LAYOUT_DESC", 0, true);

  SerialiseComplexArray("pInputElementDescs", (D3D12_INPUT_ELEMENT_DESC *&)el.pInputElementDescs,
                        el.NumElements);
}

template <>
void Serialiser::Deserialise(const D3D12_INPUT_LAYOUT_DESC *const el) const
{
  if(m_Mode == READING)
    delete[] el->pInputElementDescs;
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INDIRECT_ARGUMENT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_INDIRECT_ARGUMENT_DESC", 0, true);

  Serialise("Type", el.Type);

  switch(el.Type)
  {
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
    case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
      // nothing to serialise
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
      Serialise("VertexBuffer.Slot", el.VertexBuffer.Slot);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
      Serialise("Constant.RootParameterIndex", el.Constant.RootParameterIndex);
      Serialise("Constant.DestOffsetIn32BitValues", el.Constant.DestOffsetIn32BitValues);
      Serialise("Constant.Num32BitValuesToSet", el.Constant.Num32BitValuesToSet);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
      Serialise("ConstantBufferView.RootParameterIndex", el.ConstantBufferView.RootParameterIndex);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
      Serialise("ShaderResourceView.RootParameterIndex", el.ShaderResourceView.RootParameterIndex);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
      Serialise("UnorderedAccessView.RootParameterIndex", el.UnorderedAccessView.RootParameterIndex);
      break;
    default: RDCERR("Unexpected indirect argument type: %u", el.Type); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_SIGNATURE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_COMMAND_SIGNATURE_DESC", 0, true);

  Serialise("ByteStride", el.ByteStride);
  Serialise("NodeMask", el.NodeMask);
  SerialiseComplexArray("pArgumentDescs", (D3D12_INDIRECT_ARGUMENT_DESC *&)el.pArgumentDescs,
                        el.NumArgumentDescs);
}

template <>
void Serialiser::Deserialise(const D3D12_COMMAND_SIGNATURE_DESC *const el) const
{
  if(m_Mode == READING)
    delete[] el->pArgumentDescs;
}

template <>
void Serialiser::Serialise(const char *name, D3D12_GRAPHICS_PIPELINE_STATE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_GRAPHICS_PIPELINE_STATE_DESC", 0, true);

  SerialiseObject(ID3D12RootSignature, "pRootSignature", el.pRootSignature);
  Serialise("VS", el.VS);
  Serialise("PS", el.PS);
  Serialise("DS", el.DS);
  Serialise("HS", el.HS);
  Serialise("GS", el.GS);
  Serialise("StreamOutput", el.StreamOutput);
  Serialise("BlendState", el.BlendState);
  Serialise("SampleMask", el.SampleMask);
  Serialise("RasterizerState", el.RasterizerState);
  Serialise("DepthStencilState", el.DepthStencilState);
  Serialise("InputLayout", el.InputLayout);
  Serialise("IBStripCutValue", el.IBStripCutValue);
  Serialise("PrimitiveTopologyType", el.PrimitiveTopologyType);
  Serialise("NumRenderTargets", el.NumRenderTargets);
  SerialisePODArray<8>("RTVFormats", el.RTVFormats);
  Serialise("DSVFormat", el.DSVFormat);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("NodeMask", el.NodeMask);
  Serialise("Flags", el.Flags);

  if(m_Mode == READING)
  {
    el.CachedPSO.CachedBlobSizeInBytes = 0;
    el.CachedPSO.pCachedBlob = NULL;
  }
}

template <>
void Serialiser::Deserialise(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *const el) const
{
  if(m_Mode == READING)
  {
    delete[](byte *)(el->VS.pShaderBytecode);
    delete[](byte *)(el->PS.pShaderBytecode);
    delete[](byte *)(el->DS.pShaderBytecode);
    delete[](byte *)(el->HS.pShaderBytecode);
    delete[](byte *)(el->GS.pShaderBytecode);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_COMPUTE_PIPELINE_STATE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_COMPUTE_PIPELINE_STATE_DESC", 0, true);

  SerialiseObject(ID3D12RootSignature, "pRootSignature", el.pRootSignature);
  Serialise("CS", el.CS);
  Serialise("NodeMask", el.NodeMask);
  Serialise("Flags", el.Flags);

  if(m_Mode == READING)
  {
    el.CachedPSO.CachedBlobSizeInBytes = 0;
    el.CachedPSO.pCachedBlob = NULL;
  }
}

template <>
void Serialiser::Deserialise(const D3D12_COMPUTE_PIPELINE_STATE_DESC *const el) const
{
  if(m_Mode == READING)
    delete[](byte *)(el->CS.pShaderBytecode);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_VERTEX_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_VERTEX_BUFFER_VIEW", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = rm->GetLiveAs<ID3D12Resource>(buffer);
    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("StrideInBytes", el.StrideInBytes);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INDEX_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_INDEX_BUFFER_VIEW", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = rm->GetLiveAs<ID3D12Resource>(buffer);
    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("Format", el.Format);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_STREAM_OUTPUT_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_STREAM_OUTPUT_BUFFER_VIEW", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = rm->GetLiveAs<ID3D12Resource>(buffer);
    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferFilledSizeLocation, buffer, offs);

  Serialise("BufferFilledSizeLocation", buffer);
  Serialise("BufferFilledSizeLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = rm->GetLiveAs<ID3D12Resource>(buffer);
    if(res)
      el.BufferFilledSizeLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferFilledSizeLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_CONSTANT_BUFFER_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_CONSTANT_BUFFER_VIEW_DESC", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = NULL;

    if(buffer != ResourceId() && rm->HasLiveResource(buffer))
      res = rm->GetLiveAs<ID3D12Resource>(buffer);

    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_RESOURCE_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_SHADER_RESOURCE_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);
  // cast to a special enum so we print nicely
  Serialise("Shader4ComponentMapping", (D3D12ComponentMapping &)el.Shader4ComponentMapping);

  switch(el.ViewDimension)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_SRV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      Serialise("Buffer.StructureByteStride", el.Buffer.StructureByteStride);
      Serialise("Buffer.Flags", el.Buffer.Flags);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MostDetailedMip", el.Texture1D.MostDetailedMip);
      Serialise("Texture1D.MipLevels", el.Texture1D.MipLevels);
      Serialise("Texture1D.ResourceMinLODClamp", el.Texture1D.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MostDetailedMip", el.Texture1DArray.MostDetailedMip);
      Serialise("Texture1DArray.MipLevels", el.Texture1DArray.MipLevels);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.ResourceMinLODClamp", el.Texture1DArray.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MostDetailedMip", el.Texture2D.MostDetailedMip);
      Serialise("Texture2D.MipLevels", el.Texture2D.MipLevels);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      Serialise("Texture2D.ResourceMinLODClamp", el.Texture2D.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MostDetailedMip", el.Texture2DArray.MostDetailedMip);
      Serialise("Texture2DArray.MipLevels", el.Texture2DArray.MipLevels);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      Serialise("Texture2DArray.ResourceMinLODClamp", el.Texture2DArray.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipLevels", el.Texture3D.MipLevels);
      Serialise("Texture3D.MostDetailedMip", el.Texture3D.MostDetailedMip);
      Serialise("Texture3D.ResourceMinLODClamp", el.Texture3D.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
      Serialise("TextureCube.MostDetailedMip", el.TextureCube.MostDetailedMip);
      Serialise("TextureCube.MipLevels", el.TextureCube.MipLevels);
      Serialise("TextureCube.ResourceMinLODClamp", el.TextureCube.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
      Serialise("TextureCubeArray.MostDetailedMip", el.TextureCubeArray.MostDetailedMip);
      Serialise("TextureCubeArray.MipLevels", el.TextureCubeArray.MipLevels);
      Serialise("TextureCubeArray.First2DArrayFace", el.TextureCubeArray.First2DArrayFace);
      Serialise("TextureCubeArray.NumCubes", el.TextureCubeArray.NumCubes);
      Serialise("TextureCubeArray.ResourceMinLODClamp", el.TextureCubeArray.ResourceMinLODClamp);
      break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RENDER_TARGET_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RENDER_TARGET_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_RTV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_RTV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCIL_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DEPTH_STENCIL_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("Flags", el.Flags);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_DSV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_DSV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      break;
    default: RDCERR("Unrecognised DSV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_UNORDERED_ACCESS_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_UNORDERED_ACCESS_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_UAV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_UAV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      Serialise("Buffer.StructureByteStride", el.Buffer.StructureByteStride);
      Serialise("Buffer.CounterOffsetInBytes", el.Buffer.CounterOffsetInBytes);
      Serialise("Buffer.Flags", el.Buffer.Flags);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_BARRIER &el)
{
  ScopedContext scope(this, name, "D3D12_RESOURCE_BARRIER", 0, true);

  Serialise("Type", el.Type);
  Serialise("Flags", el.Flags);

  switch(el.Type)
  {
    case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
    {
      SerialiseObject(ID3D12Resource, "Transition.pResource", el.Transition.pResource);
      // cast to a special enum so we print 'all subresources' nicely
      RDCCOMPILE_ASSERT(sizeof(D3D12ResourceBarrierSubresource) == sizeof(UINT),
                        "Enum isn't uint sized");
      Serialise("Transition.Subresource",
                (D3D12ResourceBarrierSubresource &)el.Transition.Subresource);
      Serialise("Transition.StateBefore", el.Transition.StateBefore);
      Serialise("Transition.StateAfter", el.Transition.StateAfter);
      break;
    }
    case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
    {
      SerialiseObject(ID3D12Resource, "Aliasing.pResourceBefore", el.Aliasing.pResourceBefore);
      SerialiseObject(ID3D12Resource, "Aliasing.pResourceAfter", el.Aliasing.pResourceAfter);
      break;
    }
    case D3D12_RESOURCE_BARRIER_TYPE_UAV:
    {
      SerialiseObject(ID3D12Resource, "UAV.pResource", el.UAV.pResource);
      break;
    }
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_PROPERTIES &el)
{
  ScopedContext scope(this, name, "D3D12_HEAP_PROPERTIES", 0, true);

  Serialise("Type", el.Type);
  Serialise("CPUPageProperty", el.CPUPageProperty);
  Serialise("MemoryPoolPreference", el.MemoryPoolPreference);
  Serialise("CreationNodeMask", el.CreationNodeMask);
  Serialise("VisibleNodeMask", el.VisibleNodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_HEAP_DESC", 0, true);

  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("Properties", el.Properties);
  Serialise("Alignment", el.Alignment);
  Serialise("Flags", el.Flags);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DESCRIPTOR_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DESCRIPTOR_HEAP_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("NumDescriptors", el.NumDescriptors);
  Serialise("Flags", el.Flags);
  Serialise("NodeMask", el.NodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_QUERY_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_QUERY_HEAP_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("Count", el.Count);
  Serialise("NodeMask", el.NodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_CLEAR_VALUE &el)
{
  ScopedContext scope(this, name, "D3D12_CLEAR_VALUE", 0, true);

  Serialise("Format", el.Format);

  if(!IsDepthFormat(el.Format))
  {
    SerialisePODArray<4>("Color", el.Color);
  }
  else
  {
    Serialise("Depth", el.DepthStencil.Depth);
    Serialise("Stencil", el.DepthStencil.Stencil);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SUBRESOURCE_FOOTPRINT &el)
{
  ScopedContext scope(this, name, "D3D12_SUBRESOURCE_FOOTPRINT", 0, true);

  Serialise("Format", el.Format);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("Depth", el.Depth);
  Serialise("RowPitch", el.RowPitch);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_TEXTURE_COPY_LOCATION &el)
{
  ScopedContext scope(this, name, "D3D12_TEXTURE_COPY_LOCATION", 0, true);

  SerialiseObject(ID3D12Resource, "pResource", el.pResource);
  Serialise("Type", el.Type);

  switch(el.Type)
  {
    case D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT:
      Serialise("PlacedFootprint.Footprint", el.PlacedFootprint.Footprint);
      Serialise("PlacedFootprint.Offset", el.PlacedFootprint.Offset);
      break;
    case D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX:
      Serialise("SubresourceIndex", el.SubresourceIndex);
      break;
    default: RDCERR("Unexpected texture copy type %d", el.Type); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_TILED_RESOURCE_COORDINATE &el)
{
  ScopedContext scope(this, name, "D3D12_TILED_RESOURCE_COORDINATE", 0, true);

  Serialise("X", el.X);
  Serialise("Y", el.Y);
  Serialise("Z", el.Z);
  Serialise("Subresource", el.Subresource);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_TILE_REGION_SIZE &el)
{
  ScopedContext scope(this, name, "D3D12_TILE_REGION_SIZE", 0, true);

  Serialise("NumTiles", el.NumTiles);
  Serialise("UseBox", el.UseBox);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("Depth", el.Depth);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DISCARD_REGION &el)
{
  ScopedContext scope(this, name, "D3D12_DISCARD_REGION", 0, true);

  Serialise("FirstSubresource", el.FirstSubresource);
  Serialise("NumSubresources", el.NumSubresources);

  SerialiseComplexArray("pRects", (D3D12_RECT *&)el.pRects, el.NumRects);
}

template <>
void Serialiser::Deserialise(const D3D12_DISCARD_REGION *const el) const
{
  if(m_Mode == READING)
    delete[] el->pRects;
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SAMPLER_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_SAMPLER_DESC", 0, true);

  Serialise("Filter", el.Filter);
  Serialise("AddressU", el.AddressU);
  Serialise("AddressV", el.AddressV);
  Serialise("AddressW", el.AddressW);
  Serialise("MipLODBias", el.MipLODBias);
  Serialise("MaxAnisotropy", el.MaxAnisotropy);
  Serialise("ComparisonFunc", el.ComparisonFunc);
  SerialisePODArray<4>("BorderColor", el.BorderColor);
  Serialise("MinLOD", el.MinLOD);
  Serialise("MaxLOD", el.MaxLOD);
}

template <>
std::string DoStringise(const D3D12_VIEWPORT &el)
{
  return StringFormat::Fmt("Viewport<%.0fx%.0f+%.0f+%.0f z=%f->%f>", el.Width, el.Height,
                           el.TopLeftX, el.TopLeftY, el.MinDepth, el.MaxDepth);
}

template <>
std::string DoStringise(const D3D12_BOX &el)
{
  return StringFormat::Fmt("Box<%u,%u,%u -> %u,%u,%u>", el.left, el.top, el.front, el.right,
                           el.bottom, el.back);
}

template <>
std::string DoStringise(const PortableHandle &el)
{
  if(el.heap == ResourceId())
    return "NULL";

  return StringFormat::Fmt("D3D12_CPU_DESCRIPTOR_HANDLE(%s, %u)", ToStr(el.heap).c_str(), el.index);
}
