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

  if(list)
  {
    std::wstring text = StringFormat::UTF82Wide(marker);
    list->BeginEvent(0, text.c_str(), (UINT)text.size());
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

D3D12MarkerRegion::~D3D12MarkerRegion()
{
  if(list)
    list->EndEvent();
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

static ShaderConstant MakeConstantBufferVariable(const DXBC::CBufferVariable &var, uint32_t &offset);

static ShaderVariableType MakeShaderVariableType(DXBC::CBufferVariableType type, uint32_t &offset)
{
  ShaderVariableType ret;

  switch(type.descriptor.type)
  {
    case DXBC::VARTYPE_INT: ret.descriptor.type = VarType::Int; break;
    case DXBC::VARTYPE_BOOL:
    case DXBC::VARTYPE_UINT: ret.descriptor.type = VarType::UInt; break;
    case DXBC::VARTYPE_DOUBLE: ret.descriptor.type = VarType::Double; break;
    case DXBC::VARTYPE_FLOAT:
    default: ret.descriptor.type = VarType::Float; break;
  }
  ret.descriptor.rows = type.descriptor.rows;
  ret.descriptor.cols = type.descriptor.cols;
  ret.descriptor.elements = type.descriptor.elements;
  ret.descriptor.name = type.descriptor.name;
  ret.descriptor.rowMajorStorage = (type.descriptor.varClass == DXBC::CLASS_MATRIX_ROWS);

  uint32_t baseElemSize = (ret.descriptor.type == VarType::Double) ? 8 : 4;
  if(ret.descriptor.rowMajorStorage)
  {
    uint32_t primary = ret.descriptor.rows;
    if(primary == 3)
      primary = 4;
    ret.descriptor.arrayStride = baseElemSize * primary * ret.descriptor.cols;
  }
  else
  {
    uint32_t primary = ret.descriptor.cols;
    if(primary == 3)
      primary = 4;
    ret.descriptor.arrayStride = baseElemSize * primary * ret.descriptor.rows;
  }

  uint32_t o = offset;

  create_array_uninit(ret.members, type.members.size());
  for(size_t i = 0; i < type.members.size(); i++)
  {
    offset = o;
    ret.members[i] = MakeConstantBufferVariable(type.members[i], offset);
  }

  if(ret.members.count > 0)
  {
    ret.descriptor.rows = 0;
    ret.descriptor.cols = 0;
    ret.descriptor.elements = 0;
  }

  return ret;
}

static ShaderConstant MakeConstantBufferVariable(const DXBC::CBufferVariable &var, uint32_t &offset)
{
  ShaderConstant ret;

  ret.name = var.name;
  ret.reg.vec = offset + var.descriptor.offset / 16;
  ret.reg.comp = (var.descriptor.offset - (var.descriptor.offset & ~0xf)) / 4;
  ret.defaultValue = 0;

  offset = ret.reg.vec;

  ret.type = MakeShaderVariableType(var.type, offset);

  offset = ret.reg.vec + RDCMAX(1U, var.type.descriptor.bytesize / 16);

  return ret;
}

void MakeShaderReflection(DXBC::DXBCFile *dxbc, ShaderReflection *refl,
                          ShaderBindpointMapping *mapping)
{
  if(dxbc == NULL || !RenderDoc::Inst().IsReplayApp())
    return;

  if(dxbc->m_DebugInfo)
  {
    refl->DebugInfo.compileFlags = dxbc->m_DebugInfo->GetShaderCompileFlags();

    create_array_uninit(refl->DebugInfo.files, dxbc->m_DebugInfo->Files.size());
    for(size_t i = 0; i < dxbc->m_DebugInfo->Files.size(); i++)
    {
      refl->DebugInfo.files[i].first = dxbc->m_DebugInfo->Files[i].first;
      refl->DebugInfo.files[i].second = dxbc->m_DebugInfo->Files[i].second;
    }

    string entry = dxbc->m_DebugInfo->GetEntryFunction();
    if(entry.empty())
      entry = "main";

    // sort the file with the entry point to the start. We don't have to do anything if there's only
    // one file.
    // This isn't a perfect search - it will match entry_point() anywhere in the file, even if it's
    // in a comment or disabled preprocessor definition. This is just best-effort
    if(refl->DebugInfo.files.count > 1)
    {
      // search from 0 up. If we find a match, we swap it into [0]. If we don't find a match then
      // we can't rearrange anything. This is a no-op for 0 since it's already in first place, but
      // since our search isn't perfect we might have multiple matches with some being false
      // positives, and so we want to bias towards leaving [0] in place.
      for(int32_t i = 0; i < refl->DebugInfo.files.count; i++)
      {
        char *c = strstr(refl->DebugInfo.files[i].first.elems, entry.c_str());
        char *end = refl->DebugInfo.files[i].first.elems + refl->DebugInfo.files[i].first.count;

        // no substring match? continue
        if(c == NULL)
          continue;

        // if we did get a substring match, ensure there's whitespace preceeding it.
        if(c == entry.c_str() || !isspace((int)*(c - 1)))
          continue;

        // skip past the entry point. Then skip any whitespace
        c += entry.size();

        // check for EOF.
        if(c >= end)
          continue;

        while(c < end && isspace(*c))
          c++;

        if(c >= end)
          continue;

        // if there's an open bracket next, we found a entry_point( which we count as the
        // declaration.
        if(*c == '(')
        {
          // only do anything if we're looking at a later file
          if(i > 0)
            std::swap(refl->DebugInfo.files[0], refl->DebugInfo.files[i]);

          break;
        }
      }
    }
  }

  if(dxbc->m_ShaderBlob.empty())
    create_array_uninit(refl->RawBytes, 0);
  else
    create_array_init(refl->RawBytes, dxbc->m_ShaderBlob.size(), &dxbc->m_ShaderBlob[0]);

  refl->DispatchThreadsDimension[0] = dxbc->DispatchThreadsDimension[0];
  refl->DispatchThreadsDimension[1] = dxbc->DispatchThreadsDimension[1];
  refl->DispatchThreadsDimension[2] = dxbc->DispatchThreadsDimension[2];

  refl->InputSig = dxbc->m_InputSig;
  refl->OutputSig = dxbc->m_OutputSig;

  create_array_uninit(mapping->InputAttributes, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
  for(int s = 0; s < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; s++)
    mapping->InputAttributes[s] = s;

  int numCbuffers = 0;

  // skip 'empty' cbuffers added for the benefit of D3D11
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    if(dxbc->m_CBuffers[i].descriptor.type == DXBC::CBuffer::Descriptor::TYPE_CBUFFER)
      numCbuffers++;
  }

  create_array_uninit(mapping->ConstantBlocks, numCbuffers);
  create_array_uninit(refl->ConstantBlocks, numCbuffers);
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    ConstantBlock &cb = refl->ConstantBlocks[i];

    if(dxbc->m_CBuffers[i].descriptor.type != DXBC::CBuffer::Descriptor::TYPE_CBUFFER)
      continue;

    cb.name = dxbc->m_CBuffers[i].name;
    cb.bufferBacked = true;
    cb.byteSize = dxbc->m_CBuffers[i].descriptor.byteSize;
    cb.bindPoint = (uint32_t)i;

    BindpointMap map;
    map.arraySize = 1;
    map.bindset = dxbc->m_CBuffers[i].space;
    map.bind = dxbc->m_CBuffers[i].reg;
    map.used = true;

    mapping->ConstantBlocks[i] = map;

    create_array_uninit(cb.variables, dxbc->m_CBuffers[i].variables.size());
    for(size_t v = 0; v < dxbc->m_CBuffers[i].variables.size(); v++)
    {
      uint32_t vecOffset = 0;
      cb.variables[v] = MakeConstantBufferVariable(dxbc->m_CBuffers[i].variables[v], vecOffset);
    }
  }

  int numRWResources = 0;
  int numROResources = 0;

  for(size_t i = 0; i < dxbc->m_Resources.size(); i++)
  {
    const auto &r = dxbc->m_Resources[i];

    if(r.type != DXBC::ShaderInputBind::TYPE_CBUFFER)
    {
      bool IsReadWrite = (r.type == DXBC::ShaderInputBind::TYPE_UAV_RWTYPED ||
                          r.type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED ||
                          r.type == DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS ||
                          r.type == DXBC::ShaderInputBind::TYPE_UAV_APPEND_STRUCTURED ||
                          r.type == DXBC::ShaderInputBind::TYPE_UAV_CONSUME_STRUCTURED ||
                          r.type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER);

      if(IsReadWrite)
        numRWResources++;
      else
        numROResources++;
    }
  }

  create_array_uninit(mapping->ReadWriteResources, numRWResources);
  create_array_uninit(refl->ReadWriteResources, numRWResources);

  create_array_uninit(mapping->ReadOnlyResources, numROResources);
  create_array_uninit(refl->ReadOnlyResources, numROResources);

  int32_t rwidx = 0, roidx = 0;
  for(size_t i = 0; i < dxbc->m_Resources.size(); i++)
  {
    const auto &r = dxbc->m_Resources[i];

    if(r.type == DXBC::ShaderInputBind::TYPE_CBUFFER)
      continue;

    ShaderResource res;
    res.name = r.name;

    res.IsSampler = (r.type == DXBC::ShaderInputBind::TYPE_SAMPLER);
    res.IsTexture = (r.type == DXBC::ShaderInputBind::TYPE_TEXTURE &&
                     r.dimension != DXBC::ShaderInputBind::DIM_UNKNOWN &&
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFER &&
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFEREX);
    res.IsReadOnly = (r.type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
                      r.type == DXBC::ShaderInputBind::TYPE_SAMPLER ||
                      r.type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
                      r.type == DXBC::ShaderInputBind::TYPE_STRUCTURED ||
                      r.type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS);

    switch(r.dimension)
    {
      default:
      case DXBC::ShaderInputBind::DIM_UNKNOWN: res.resType = TextureDim::Unknown; break;
      case DXBC::ShaderInputBind::DIM_BUFFER:
      case DXBC::ShaderInputBind::DIM_BUFFEREX: res.resType = TextureDim::Buffer; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE1D: res.resType = TextureDim::Texture1D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY:
        res.resType = TextureDim::Texture1DArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2D: res.resType = TextureDim::Texture2D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY:
        res.resType = TextureDim::Texture2DArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DMS: res.resType = TextureDim::Texture2DMS; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY:
        res.resType = TextureDim::Texture2DMSArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE3D: res.resType = TextureDim::Texture3D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURECUBE: res.resType = TextureDim::TextureCube; break;
      case DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY:
        res.resType = TextureDim::TextureCubeArray;
        break;
    }

    if(r.retType != DXBC::ShaderInputBind::RETTYPE_UNKNOWN &&
       r.retType != DXBC::ShaderInputBind::RETTYPE_MIXED &&
       r.retType != DXBC::ShaderInputBind::RETTYPE_CONTINUED)
    {
      res.variableType.descriptor.rows = 1;
      res.variableType.descriptor.cols = r.numSamples;
      res.variableType.descriptor.elements = 1;

      string name;

      switch(r.retType)
      {
        case DXBC::ShaderInputBind::RETTYPE_UNORM: name = "unorm float"; break;
        case DXBC::ShaderInputBind::RETTYPE_SNORM: name = "snorm float"; break;
        case DXBC::ShaderInputBind::RETTYPE_SINT: name = "int"; break;
        case DXBC::ShaderInputBind::RETTYPE_UINT: name = "uint"; break;
        case DXBC::ShaderInputBind::RETTYPE_FLOAT: name = "float"; break;
        case DXBC::ShaderInputBind::RETTYPE_DOUBLE: name = "double"; break;
        default: name = "unknown"; break;
      }

      name += ToStr::Get(r.numSamples);

      res.variableType.descriptor.name = name;
    }
    else
    {
      if(dxbc->m_ResourceBinds.find(r.name) != dxbc->m_ResourceBinds.end())
      {
        uint32_t vecOffset = 0;
        res.variableType = MakeShaderVariableType(dxbc->m_ResourceBinds[r.name], vecOffset);
      }
      else
      {
        res.variableType.descriptor.rows = 0;
        res.variableType.descriptor.cols = 0;
        res.variableType.descriptor.elements = 0;
        res.variableType.descriptor.name = "";
      }
    }

    res.bindPoint = res.IsReadOnly ? roidx : rwidx;

    BindpointMap map;
    map.arraySize = r.bindCount == 0 ? ~0U : r.bindCount;
    map.bindset = r.space;
    map.bind = r.reg;
    map.used = true;

    if(res.IsReadOnly)
    {
      mapping->ReadOnlyResources[roidx] = map;
      refl->ReadOnlyResources[roidx++] = res;
    }
    else
    {
      mapping->ReadWriteResources[rwidx] = map;
      refl->ReadWriteResources[rwidx++] = res;
    }
  }

  uint32_t numInterfaces = 0;
  for(size_t i = 0; i < dxbc->m_Interfaces.variables.size(); i++)
    numInterfaces = RDCMAX(dxbc->m_Interfaces.variables[i].descriptor.offset + 1, numInterfaces);

  create_array(refl->Interfaces, numInterfaces);
  for(size_t i = 0; i < dxbc->m_Interfaces.variables.size(); i++)
    refl->Interfaces[dxbc->m_Interfaces.variables[i].descriptor.offset] =
        dxbc->m_Interfaces.variables[i].name;
}

enum D3D12ResourceBarrierSubresource
{
  D3D12AllSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
};

string ToStrHelper<false, D3D12ResourceBarrierSubresource>::Get(const D3D12ResourceBarrierSubresource &el)
{
  if(el == D3D12AllSubresources)
    return "All Subresources";

  return ToStr::Get(uint32_t(el));
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

string ToStrHelper<false, D3D12ComponentMapping>::Get(const D3D12ComponentMapping &el)
{
  string ret;

  // value should always be <= 5, see D3D12_SHADER_COMPONENT_MAPPING
  const char mapping[] = {'R', 'G', 'B', 'A', '0', '1', '?', '?'};

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

string ToStrHelper<false, D3D12_VIEWPORT>::Get(const D3D12_VIEWPORT &el)
{
  return StringFormat::Fmt("Viewport<%.0fx%.0f+%.0f+%.0f z=%f->%f>", el.Width, el.Height,
                           el.TopLeftX, el.TopLeftY, el.MinDepth, el.MaxDepth);
}

string ToStrHelper<false, D3D12_BOX>::Get(const D3D12_BOX &el)
{
  return StringFormat::Fmt("Box<%u,%u,%u -> %u,%u,%u>", el.left, el.top, el.front, el.right,
                           el.bottom, el.back);
}

string ToStrHelper<false, PortableHandle>::Get(const PortableHandle &el)
{
  if(el.heap == ResourceId())
    return "NULL";

  return StringFormat::Fmt("D3D12_CPU_DESCRIPTOR_HANDLE(%s, %u)", ToStr::Get(el.heap).c_str(),
                           el.index);
}

string ToStrHelper<false, D3D12Descriptor::DescriptorType>::Get(const D3D12Descriptor::DescriptorType &el)
{
  switch(el)
  {
    case D3D12Descriptor::TypeCBV: return "CBV";
    case D3D12Descriptor::TypeSRV: return "SRV";
    case D3D12Descriptor::TypeUAV: return "UAV";
    case D3D12Descriptor::TypeRTV: return "RTV";
    case D3D12Descriptor::TypeDSV: return "DSV";
    case D3D12Descriptor::TypeUndefined: return "Undefined";
    default: break;
  }

  if((uint32_t)el < D3D12Descriptor::TypeCBV)
    return "Sampler";

  return StringFormat::Fmt("DescriptorType<%d>", el);
}

string ToStrHelper<false, D3D12ResourceType>::Get(const D3D12ResourceType &el)
{
  switch(el)
  {
    case Resource_Device: return "Device";
    case Resource_Unknown: return "Unknown";
    case Resource_CommandAllocator: return "Command Allocator";
    case Resource_CommandQueue: return "Command Queue";
    case Resource_CommandSignature: return "Command Signature";
    case Resource_DescriptorHeap: return "Descriptor Heap";
    case Resource_Fence: return "Fence";
    case Resource_Heap: return "Heap";
    case Resource_PipelineState: return "Pipeline State";
    case Resource_QueryHeap: return "Query Heap";
    case Resource_Resource: return "Resource";
    case Resource_GraphicsCommandList: return "Graphics CommandList";
    case Resource_RootSignature: return "Root Signature";
    default: break;
  }

  return StringFormat::Fmt("D3D12ResourceType<%d>", el);
}

string ToStrHelper<false, D3D12_HEAP_TYPE>::Get(const D3D12_HEAP_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_DEFAULT)
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_UPLOAD)
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_READBACK)
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_CUSTOM)
    default: break;
  }

  return StringFormat::Fmt("D3D12_HEAP_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_CPU_PAGE_PROPERTY>::Get(const D3D12_CPU_PAGE_PROPERTY &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE)
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK)
    default: break;
  }

  return StringFormat::Fmt("D3D12_CPU_PAGE_PROPERTY<%d>", el);
}

string ToStrHelper<false, D3D12_MEMORY_POOL>::Get(const D3D12_MEMORY_POOL &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_MEMORY_POOL_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_MEMORY_POOL_L0)
    TOSTR_CASE_STRINGIZE(D3D12_MEMORY_POOL_L1)
    default: break;
  }

  return StringFormat::Fmt("D3D12_MEMORY_POOL<%d>", el);
}

string ToStrHelper<false, D3D12_QUERY_HEAP_TYPE>::Get(const D3D12_QUERY_HEAP_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_OCCLUSION)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_TIMESTAMP)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_SO_STATISTICS)
    default: break;
  }

  return StringFormat::Fmt("D3D12_QUERY_HEAP_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_QUERY_TYPE>::Get(const D3D12_QUERY_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_OCCLUSION)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_BINARY_OCCLUSION)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_TIMESTAMP)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_PIPELINE_STATISTICS)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3)
    default: break;
  }

  return StringFormat::Fmt("D3D12_QUERY_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_PREDICATION_OP>::Get(const D3D12_PREDICATION_OP &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_PREDICATION_OP_EQUAL_ZERO)
    TOSTR_CASE_STRINGIZE(D3D12_PREDICATION_OP_NOT_EQUAL_ZERO)
    default: break;
  }

  return StringFormat::Fmt("D3D12_PREDICATION_OP<%d>", el);
}

string ToStrHelper<false, D3D12_DESCRIPTOR_HEAP_TYPE>::Get(const D3D12_DESCRIPTOR_HEAP_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    default: break;
  }

  return StringFormat::Fmt("D3D12_DESCRIPTOR_HEAP_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_RESOURCE_BARRIER_TYPE>::Get(const D3D12_RESOURCE_BARRIER_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_BARRIER_TYPE_UAV)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RESOURCE_BARRIER_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_SRV_DIMENSION>::Get(const D3D12_SRV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE3D)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURECUBE)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURECUBEARRAY)
    default: break;
  }

  return StringFormat::Fmt("D3D12_SRV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_RTV_DIMENSION>::Get(const D3D12_RTV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RTV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_UAV_DIMENSION>::Get(const D3D12_UAV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_UAV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_DSV_DIMENSION>::Get(const D3D12_DSV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY)
    default: break;
  }

  return StringFormat::Fmt("D3D12_DSV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_FILTER>::Get(const D3D12_FILTER &el)
{
  switch(el)
  {
    case D3D12_FILTER_MIN_MAG_MIP_POINT: return "MIN_MAG_MIP_POINT";
    case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR: return "MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT: return "MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR: return "MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT: return "MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return "MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT: return "MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MIN_MAG_MIP_LINEAR: return "MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_ANISOTROPIC: return "ANISOTROPIC";

    case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT: return "CMP:MIN_MAG_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR: return "CMP:MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "CMP:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR: return "CMP:MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT: return "CMP:MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "CMP:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT: return "CMP:MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR: return "CMP:MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_ANISOTROPIC: return "CMP:ANISOTROPIC";

    case D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT: return "MIN:MIN_MAG_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR: return "MIN:MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "MIN:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR: return "MIN:MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT: return "MIN:MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "MIN:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT: return "MIN:MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR: return "MIN:MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_ANISOTROPIC: return "MIN:ANISOTROPIC";

    case D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT: return "MAX:MIN_MAG_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR: return "MAX:MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "MAX:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR: return "MAX:MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT: return "MAX:MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "MAX:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT: return "MAX:MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR: return "MAX:MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_ANISOTROPIC: return "MAX:ANISOTROPIC";

    default: break;
  }

  return StringFormat::Fmt("D3D12_FILTER<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_ADDRESS_MODE>::Get(const D3D12_TEXTURE_ADDRESS_MODE &el)
{
  switch(el)
  {
    case D3D12_TEXTURE_ADDRESS_MODE_WRAP: return "WRAP";
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR: return "MIRROR";
    case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: return "CLAMP";
    case D3D12_TEXTURE_ADDRESS_MODE_BORDER: return "BORDER";
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return "MIRROR_ONCE";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_TEXTURE_ADDRESS_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_BLEND>::Get(const D3D12_BLEND &el)
{
  switch(el)
  {
    case D3D12_BLEND_ZERO: return "ZERO";
    case D3D12_BLEND_ONE: return "ONE";
    case D3D12_BLEND_SRC_COLOR: return "SRC_COLOR";
    case D3D12_BLEND_INV_SRC_COLOR: return "INV_SRC_COLOR";
    case D3D12_BLEND_SRC_ALPHA: return "SRC_ALPHA";
    case D3D12_BLEND_INV_SRC_ALPHA: return "INV_SRC_ALPHA";
    case D3D12_BLEND_DEST_ALPHA: return "DEST_ALPHA";
    case D3D12_BLEND_INV_DEST_ALPHA: return "INV_DEST_ALPHA";
    case D3D12_BLEND_DEST_COLOR: return "DEST_COLOR";
    case D3D12_BLEND_INV_DEST_COLOR: return "INV_DEST_COLOR";
    case D3D12_BLEND_SRC_ALPHA_SAT: return "SRC_ALPHA_SAT";
    case D3D12_BLEND_BLEND_FACTOR: return "BLEND_FACTOR";
    case D3D12_BLEND_INV_BLEND_FACTOR: return "INV_BLEND_FACTOR";
    case D3D12_BLEND_SRC1_COLOR: return "SRC1_COLOR";
    case D3D12_BLEND_INV_SRC1_COLOR: return "INV_SRC1_COLOR";
    case D3D12_BLEND_SRC1_ALPHA: return "SRC1_ALPHA";
    case D3D12_BLEND_INV_SRC1_ALPHA: return "INV_SRC1_ALPHA";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_BLEND<%d>", el);
}

string ToStrHelper<false, D3D12_BLEND_OP>::Get(const D3D12_BLEND_OP &el)
{
  switch(el)
  {
    case D3D12_BLEND_OP_ADD: return "ADD";
    case D3D12_BLEND_OP_SUBTRACT: return "SUBTRACT";
    case D3D12_BLEND_OP_REV_SUBTRACT: return "REV_SUBTRACT";
    case D3D12_BLEND_OP_MIN: return "MIN";
    case D3D12_BLEND_OP_MAX: return "MAX";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_BLEND_OP<%d>", el);
}

string ToStrHelper<false, D3D12_LOGIC_OP>::Get(const D3D12_LOGIC_OP &el)
{
  switch(el)
  {
    case D3D12_LOGIC_OP_CLEAR: return "CLEAR";
    case D3D12_LOGIC_OP_SET: return "SET";
    case D3D12_LOGIC_OP_COPY: return "COPY";
    case D3D12_LOGIC_OP_COPY_INVERTED: return "COPY_INVERTED";
    case D3D12_LOGIC_OP_NOOP: return "NOOP";
    case D3D12_LOGIC_OP_INVERT: return "INVERT";
    case D3D12_LOGIC_OP_AND: return "AND";
    case D3D12_LOGIC_OP_NAND: return "NAND";
    case D3D12_LOGIC_OP_OR: return "OR";
    case D3D12_LOGIC_OP_NOR: return "NOR";
    case D3D12_LOGIC_OP_XOR: return "XOR";
    case D3D12_LOGIC_OP_EQUIV: return "EQUIV";
    case D3D12_LOGIC_OP_AND_REVERSE: return "AND_REVERSE";
    case D3D12_LOGIC_OP_AND_INVERTED: return "AND_INVERTED";
    case D3D12_LOGIC_OP_OR_REVERSE: return "OR_REVERSE";
    case D3D12_LOGIC_OP_OR_INVERTED: return "OR_INVERTED";
    default: break;
  }

  return StringFormat::Fmt("D3D12_LOGIC_OP<%d>", el);
}

string ToStrHelper<false, D3D12_FILL_MODE>::Get(const D3D12_FILL_MODE &el)
{
  switch(el)
  {
    case D3D12_FILL_MODE_WIREFRAME: return "WIREFRAME";
    case D3D12_FILL_MODE_SOLID: return "SOLID";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_FILL_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_CULL_MODE>::Get(const D3D12_CULL_MODE &el)
{
  switch(el)
  {
    case D3D12_CULL_MODE_NONE: return "NONE";
    case D3D12_CULL_MODE_FRONT: return "FRONT";
    case D3D12_CULL_MODE_BACK: return "BACK";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_CULL_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_CONSERVATIVE_RASTERIZATION_MODE>::Get(
    const D3D12_CONSERVATIVE_RASTERIZATION_MODE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF)
    TOSTR_CASE_STRINGIZE(D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON)
    default: break;
  }

  return StringFormat::Fmt("D3D12_CONSERVATIVE_RASTERIZATION_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_COMPARISON_FUNC>::Get(const D3D12_COMPARISON_FUNC &el)
{
  switch(el)
  {
    case D3D12_COMPARISON_FUNC_NEVER: return "NEVER";
    case D3D12_COMPARISON_FUNC_LESS: return "LESS";
    case D3D12_COMPARISON_FUNC_EQUAL: return "EQUAL";
    case D3D12_COMPARISON_FUNC_LESS_EQUAL: return "LESS_EQUAL";
    case D3D12_COMPARISON_FUNC_GREATER: return "GREATER";
    case D3D12_COMPARISON_FUNC_NOT_EQUAL: return "NOT_EQUAL";
    case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return "GREATER_EQUAL";
    case D3D12_COMPARISON_FUNC_ALWAYS: return "ALWAYS";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_COMPARISON_FUNC<%d>", el);
}

string ToStrHelper<false, D3D12_DEPTH_WRITE_MASK>::Get(const D3D12_DEPTH_WRITE_MASK &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_DEPTH_WRITE_MASK_ZERO)
    TOSTR_CASE_STRINGIZE(D3D12_DEPTH_WRITE_MASK_ALL)
    default: break;
  }

  return StringFormat::Fmt("D3D12_DEPTH_WRITE_MASK<%d>", el);
}

string ToStrHelper<false, D3D12_STENCIL_OP>::Get(const D3D12_STENCIL_OP &el)
{
  switch(el)
  {
    case D3D12_STENCIL_OP_KEEP: return "KEEP";
    case D3D12_STENCIL_OP_ZERO: return "ZERO";
    case D3D12_STENCIL_OP_REPLACE: return "REPLACE";
    case D3D12_STENCIL_OP_INCR_SAT: return "INCR_SAT";
    case D3D12_STENCIL_OP_DECR_SAT: return "DECR_SAT";
    case D3D12_STENCIL_OP_INVERT: return "INVERT";
    case D3D12_STENCIL_OP_INCR: return "INCR";
    case D3D12_STENCIL_OP_DECR: return "DECR";
    default: break;
  }

  // possible for unused fields via 0-initialisation
  if((int)el == 0)
    return "--";

  return StringFormat::Fmt("D3D12_STENCIL_OP<%d>", el);
}

string ToStrHelper<false, D3D12_INPUT_CLASSIFICATION>::Get(const D3D12_INPUT_CLASSIFICATION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA)
    TOSTR_CASE_STRINGIZE(D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA)
    default: break;
  }

  return StringFormat::Fmt("D3D12_INPUT_CLASSIFICATION<%d>", el);
}

string ToStrHelper<false, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>::Get(
    const D3D12_INDEX_BUFFER_STRIP_CUT_VALUE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED)
    TOSTR_CASE_STRINGIZE(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF)
    TOSTR_CASE_STRINGIZE(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF)
    default: break;
  }

  return StringFormat::Fmt("D3D12_INDEX_BUFFER_STRIP_CUT_VALUE<%d>", el);
}

string ToStrHelper<false, D3D12_PRIMITIVE_TOPOLOGY_TYPE>::Get(const D3D12_PRIMITIVE_TOPOLOGY_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
    default: break;
  }

  return StringFormat::Fmt("D3D12_PRIMITIVE_TOPOLOGY_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_INDIRECT_ARGUMENT_TYPE>::Get(const D3D12_INDIRECT_ARGUMENT_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
    default: break;
  }

  return StringFormat::Fmt("D3D12_INDIRECT_ARGUMENT_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_COMMAND_LIST_TYPE>::Get(const D3D12_COMMAND_LIST_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_DIRECT)
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_BUNDLE)
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_COMPUTE)
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_COPY)
    default: break;
  }

  return StringFormat::Fmt("D3D12_COMMAND_LIST_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_COPY_TYPE>::Get(const D3D12_TEXTURE_COPY_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)
    default: break;
  }

  return StringFormat::Fmt("D3D12_TEXTURE_COPY_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_RESOURCE_DIMENSION>::Get(const D3D12_RESOURCE_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RESOURCE_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_LAYOUT>::Get(const D3D12_TEXTURE_LAYOUT &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_ROW_MAJOR)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE)
    default: break;
  }

  return StringFormat::Fmt("D3D12_TEXTURE_LAYOUT<%d>", el);
}

string ToStrHelper<false, D3D12_CLEAR_FLAGS>::Get(const D3D12_CLEAR_FLAGS &el)
{
  string ret;

  if(el & D3D12_CLEAR_FLAG_DEPTH)
    ret += " | D3D12_CLEAR_FLAG_DEPTH";

  if(el & D3D12_CLEAR_FLAG_STENCIL)
    ret += " | D3D12_CLEAR_FLAG_STENCIL";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_BUFFER_SRV_FLAGS>::Get(const D3D12_BUFFER_SRV_FLAGS &el)
{
  string ret;

  if(el == D3D12_BUFFER_SRV_FLAG_NONE)
    return "D3D12_BUFFER_SRV_FLAG_NONE";

  if(el & D3D12_BUFFER_SRV_FLAG_RAW)
    ret += " | D3D12_BUFFER_SRV_FLAG_RAW";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_DSV_FLAGS>::Get(const D3D12_DSV_FLAGS &el)
{
  string ret;

  if(el == D3D12_DSV_FLAG_NONE)
    return "D3D12_DSV_FLAG_NONE";

  if(el & D3D12_DSV_FLAG_READ_ONLY_DEPTH)
    ret += " | D3D12_DSV_FLAG_READ_ONLY_DEPTH";
  if(el & D3D12_DSV_FLAG_READ_ONLY_STENCIL)
    ret += " | D3D12_DSV_FLAG_READ_ONLY_STENCIL";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_BUFFER_UAV_FLAGS>::Get(const D3D12_BUFFER_UAV_FLAGS &el)
{
  string ret;

  if(el == D3D12_BUFFER_UAV_FLAG_NONE)
    return "D3D12_BUFFER_UAV_FLAG_NONE";

  if(el & D3D12_BUFFER_UAV_FLAG_RAW)
    ret += " | D3D12_BUFFER_UAV_FLAG_RAW";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_HEAP_FLAGS>::Get(const D3D12_HEAP_FLAGS &el)
{
  string ret;

  if(el == D3D12_HEAP_FLAG_NONE)
    return "D3D12_HEAP_FLAG_NONE";

  if(el & D3D12_HEAP_FLAG_SHARED)
    ret += " | D3D12_HEAP_FLAG_SHARED";
  if(el & D3D12_HEAP_FLAG_DENY_BUFFERS)
    ret += " | D3D12_HEAP_FLAG_DENY_BUFFERS";
  if(el & D3D12_HEAP_FLAG_ALLOW_DISPLAY)
    ret += " | D3D12_HEAP_FLAG_ALLOW_DISPLAY";
  if(el & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER)
    ret += " | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER";
  if(el & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES";
  if(el & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES";
  if(el & D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES";
  if(el & D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS";
  if(el & D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES";
  if(el & D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_FENCE_FLAGS>::Get(const D3D12_FENCE_FLAGS &el)
{
  string ret;

  if(el == D3D12_FENCE_FLAG_NONE)
    return "D3D12_FENCE_FLAG_NONE";

  if(el & D3D12_FENCE_FLAG_SHARED)
    ret += " | D3D12_FENCE_FLAG_SHARED";
  if(el & D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER)
    ret += " | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_DESCRIPTOR_HEAP_FLAGS>::Get(const D3D12_DESCRIPTOR_HEAP_FLAGS &el)
{
  string ret;

  if(el == D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
    return "D3D12_DESCRIPTOR_HEAP_FLAG_NONE";

  if(el & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    ret += " | D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_RESOURCE_BARRIER_FLAGS>::Get(const D3D12_RESOURCE_BARRIER_FLAGS &el)
{
  string ret;

  if(el == D3D12_RESOURCE_BARRIER_FLAG_NONE)
    return "D3D12_RESOURCE_BARRIER_FLAG_NONE";

  if(el & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
    ret += " | D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY";
  if(el & D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
    ret += " | D3D12_RESOURCE_BARRIER_FLAG_END_ONLY";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_RESOURCE_STATES>::Get(const D3D12_RESOURCE_STATES &el)
{
  string ret;

  if(el == D3D12_RESOURCE_STATE_COMMON)
    return "COMMON/PRESENT";

  if(el == D3D12_RESOURCE_STATE_GENERIC_READ)
    return "GENERIC_READ";

  if(el & D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
    ret += " | VB & CB";
  if(el & D3D12_RESOURCE_STATE_INDEX_BUFFER)
    ret += " | IB";
  if(el & D3D12_RESOURCE_STATE_RENDER_TARGET)
    ret += " | RTV";
  if(el & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    ret += " | UAV";
  if(el & D3D12_RESOURCE_STATE_DEPTH_WRITE)
    ret += " | DSV Write";
  if(el & D3D12_RESOURCE_STATE_DEPTH_READ)
    ret += " | DSV Read";
  if(el & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    ret += " | SRV (Non-Pixel)";
  if(el & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    ret += " | SRV (Pixel)";
  if(el & D3D12_RESOURCE_STATE_STREAM_OUT)
    ret += " | Stream Out";
  if(el & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
    ret += " | Indirect";
  if(el & D3D12_RESOURCE_STATE_COPY_DEST)
    ret += " | Copy (Dst)";
  if(el & D3D12_RESOURCE_STATE_COPY_SOURCE)
    ret += " | Copy (Src)";
  if(el & D3D12_RESOURCE_STATE_RESOLVE_DEST)
    ret += " | Resolve (Dst)";
  if(el & D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
    ret += " | Resolve (Src)";
  if(el & D3D12_RESOURCE_STATE_PREDICATION)
    ret += " | Predication";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_PIPELINE_STATE_FLAGS>::Get(const D3D12_PIPELINE_STATE_FLAGS &el)
{
  string ret;

  if(el == D3D12_PIPELINE_STATE_FLAG_NONE)
    return "D3D12_PIPELINE_STATE_FLAG_NONE";

  if(el & D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG)
    ret += " | D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_RESOURCE_FLAGS>::Get(const D3D12_RESOURCE_FLAGS &el)
{
  string ret;

  if(el == D3D12_RESOURCE_FLAG_NONE)
    return "D3D12_RESOURCE_FLAG_NONE";

  if(el & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS";
  if(el & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
    ret += " | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_COMMAND_QUEUE_FLAGS>::Get(const D3D12_COMMAND_QUEUE_FLAGS &el)
{
  string ret;

  if(el == D3D12_COMMAND_QUEUE_FLAG_NONE)
    return "D3D12_COMMAND_QUEUE_FLAG_NONE";

  if(el & D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
    ret += " | D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_TILE_COPY_FLAGS>::Get(const D3D12_TILE_COPY_FLAGS &el)
{
  string ret;

  if(el == D3D12_TILE_COPY_FLAG_NONE)
    return "D3D12_TILE_COPY_FLAG_NONE";

  if(el & D3D12_TILE_COPY_FLAG_NO_HAZARD)
    ret += " | D3D12_TILE_COPY_FLAG_NO_HAZARD";

  if(el & D3D12_TILE_COPY_FLAG_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE)
    ret += " | D3D12_TILE_COPY_FLAG_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE";

  if(el & D3D12_TILE_COPY_FLAG_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER)
    ret += " | D3D12_TILE_COPY_FLAG_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_TILE_MAPPING_FLAGS>::Get(const D3D12_TILE_MAPPING_FLAGS &el)
{
  string ret;

  if(el == D3D12_TILE_MAPPING_FLAG_NONE)
    return "D3D12_TILE_MAPPING_FLAG_NONE";

  if(el & D3D12_TILE_MAPPING_FLAG_NO_HAZARD)
    ret += " | D3D12_TILE_MAPPING_FLAG_NO_HAZARD";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_TILE_RANGE_FLAGS>::Get(const D3D12_TILE_RANGE_FLAGS &el)
{
  string ret;

  if(el == D3D12_TILE_RANGE_FLAG_NONE)
    return "D3D12_TILE_RANGE_FLAG_NONE";

  if(el & D3D12_TILE_RANGE_FLAG_NULL)
    ret += " | D3D12_TILE_RANGE_FLAG_NULL";

  if(el & D3D12_TILE_RANGE_FLAG_SKIP)
    ret += " | D3D12_TILE_RANGE_FLAG_SKIP";

  if(el & D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE)
    ret += " | D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_TILED_RESOURCES_TIER>::Get(const D3D12_TILED_RESOURCES_TIER &el)
{
  string ret;

  if(el == D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED)
    return "D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED";

  if(el & D3D12_TILED_RESOURCES_TIER_1)
    ret += " | D3D12_TILED_RESOURCES_TIER_1";

  if(el & D3D12_TILED_RESOURCES_TIER_2)
    ret += " | D3D12_TILED_RESOURCES_TIER_2";

  if(el & D3D12_TILED_RESOURCES_TIER_3)
    ret += " | D3D12_TILED_RESOURCES_TIER_3";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}
