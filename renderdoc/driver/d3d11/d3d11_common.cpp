/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "serialise/string_utils.h"

WrappedID3D11Device *D3D11MarkerRegion::device;

D3D11MarkerRegion::D3D11MarkerRegion(const std::string &marker)
{
  if(device == NULL)
    return;

  ID3DUserDefinedAnnotation *annot = device->GetAnnotations();

  if(annot)
    annot->BeginEvent(StringFormat::UTF82Wide(marker).c_str());
}

void D3D11MarkerRegion::Set(const std::string &marker)
{
  if(device == NULL)
    return;

  ID3DUserDefinedAnnotation *annot = device->GetAnnotations();

  if(annot)
    annot->SetMarker(StringFormat::UTF82Wide(marker).c_str());
}

D3D11MarkerRegion::~D3D11MarkerRegion()
{
  if(device == NULL)
    return;

  ID3DUserDefinedAnnotation *annot = device->GetAnnotations();

  if(annot)
    annot->EndEvent();
}

TextureDim MakeTextureDim(D3D11_SRV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_SRV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D11_SRV_DIMENSION_BUFFER:
    case D3D11_SRV_DIMENSION_BUFFEREX: return TextureDim::Buffer;
    case D3D11_SRV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D11_SRV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS: return TextureDim::Texture2DMS;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY: return TextureDim::Texture2DMSArray;
    case D3D11_SRV_DIMENSION_TEXTURE3D: return TextureDim::Texture3D;
    case D3D11_SRV_DIMENSION_TEXTURECUBE: return TextureDim::TextureCube;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: return TextureDim::TextureCubeArray;
  }

  return TextureDim::Unknown;
}

TextureDim MakeTextureDim(D3D11_RTV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_RTV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D11_RTV_DIMENSION_BUFFER: return TextureDim::Buffer;
    case D3D11_RTV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D11_RTV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS: return TextureDim::Texture2DMS;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: return TextureDim::Texture2DMSArray;
    case D3D11_RTV_DIMENSION_TEXTURE3D: return TextureDim::Texture3D;
  }

  return TextureDim::Unknown;
}

TextureDim MakeTextureDim(D3D11_DSV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_DSV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D11_DSV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D11_DSV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS: return TextureDim::Texture2DMS;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: return TextureDim::Texture2DMSArray;
  }

  return TextureDim::Unknown;
}

TextureDim MakeTextureDim(D3D11_UAV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D11_UAV_DIMENSION_UNKNOWN: return TextureDim::Unknown;
    case D3D11_UAV_DIMENSION_BUFFER: return TextureDim::Buffer;
    case D3D11_UAV_DIMENSION_TEXTURE1D: return TextureDim::Texture1D;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: return TextureDim::Texture1DArray;
    case D3D11_UAV_DIMENSION_TEXTURE2D: return TextureDim::Texture2D;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: return TextureDim::Texture2DArray;
    case D3D11_UAV_DIMENSION_TEXTURE3D: return TextureDim::Texture3D;
  }

  return TextureDim::Unknown;
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

CompareFunc MakeCompareFunc(D3D11_COMPARISON_FUNC func)
{
  switch(func)
  {
    case D3D11_COMPARISON_NEVER: return CompareFunc::Never;
    case D3D11_COMPARISON_LESS: return CompareFunc::Less;
    case D3D11_COMPARISON_EQUAL: return CompareFunc::Equal;
    case D3D11_COMPARISON_LESS_EQUAL: return CompareFunc::LessEqual;
    case D3D11_COMPARISON_GREATER: return CompareFunc::Greater;
    case D3D11_COMPARISON_NOT_EQUAL: return CompareFunc::NotEqual;
    case D3D11_COMPARISON_GREATER_EQUAL: return CompareFunc::GreaterEqual;
    case D3D11_COMPARISON_ALWAYS: return CompareFunc::AlwaysTrue;
    default: break;
  }

  return CompareFunc::AlwaysTrue;
}

TextureFilter MakeFilter(D3D11_FILTER filter)
{
  TextureFilter ret;

  ret.func = FilterFunc::Normal;

  if(filter >= D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT &&
     filter < D3D11_FILTER_COMPARISON_ANISOTROPIC)
  {
    ret.func = FilterFunc::Comparison;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D11_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D11_FILTER_MINIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D11_FILTER_MINIMUM_ANISOTROPIC)
  {
    ret.func = FilterFunc::Minimum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D11_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_POINT &&
          filter < D3D11_FILTER_MAXIMUM_ANISOTROPIC)
  {
    ret.func = FilterFunc::Maximum;
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

LogicOp MakeLogicOp(D3D11_LOGIC_OP op)
{
  switch(op)
  {
    case D3D11_LOGIC_OP_CLEAR: return LogicOp::Clear;
    case D3D11_LOGIC_OP_AND: return LogicOp::And;
    case D3D11_LOGIC_OP_AND_REVERSE: return LogicOp::AndReverse;
    case D3D11_LOGIC_OP_COPY: return LogicOp::Copy;
    case D3D11_LOGIC_OP_AND_INVERTED: return LogicOp::AndInverted;
    case D3D11_LOGIC_OP_NOOP: return LogicOp::NoOp;
    case D3D11_LOGIC_OP_XOR: return LogicOp::Xor;
    case D3D11_LOGIC_OP_OR: return LogicOp::Or;
    case D3D11_LOGIC_OP_NOR: return LogicOp::Nor;
    case D3D11_LOGIC_OP_EQUIV: return LogicOp::Equivalent;
    case D3D11_LOGIC_OP_INVERT: return LogicOp::Invert;
    case D3D11_LOGIC_OP_OR_REVERSE: return LogicOp::OrReverse;
    case D3D11_LOGIC_OP_COPY_INVERTED: return LogicOp::CopyInverted;
    case D3D11_LOGIC_OP_OR_INVERTED: return LogicOp::OrInverted;
    case D3D11_LOGIC_OP_NAND: return LogicOp::Nand;
    case D3D11_LOGIC_OP_SET: return LogicOp::Set;
    default: break;
  }

  return LogicOp::NoOp;
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

BlendOp MakeBlendOp(D3D11_BLEND_OP op)
{
  switch(op)
  {
    case D3D11_BLEND_OP_ADD: return BlendOp::Add;
    case D3D11_BLEND_OP_SUBTRACT: return BlendOp::Subtract;
    case D3D11_BLEND_OP_REV_SUBTRACT: return BlendOp::ReversedSubtract;
    case D3D11_BLEND_OP_MIN: return BlendOp::Minimum;
    case D3D11_BLEND_OP_MAX: return BlendOp::Maximum;
    default: break;
  }

  return BlendOp::Add;
}

StencilOp MakeStencilOp(D3D11_STENCIL_OP op)
{
  switch(op)
  {
    case D3D11_STENCIL_OP_KEEP: return StencilOp::Keep;
    case D3D11_STENCIL_OP_ZERO: return StencilOp::Zero;
    case D3D11_STENCIL_OP_REPLACE: return StencilOp::Replace;
    case D3D11_STENCIL_OP_INCR_SAT: return StencilOp::IncSat;
    case D3D11_STENCIL_OP_DECR_SAT: return StencilOp::DecSat;
    case D3D11_STENCIL_OP_INVERT: return StencilOp::Invert;
    case D3D11_STENCIL_OP_INCR: return StencilOp::IncWrap;
    case D3D11_STENCIL_OP_DECR: return StencilOp::DecWrap;
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

ShaderReflection *MakeShaderReflection(DXBC::DXBCFile *dxbc)
{
  if(dxbc == NULL || !RenderDoc::Inst().IsReplayApp())
    return NULL;

  ShaderReflection *ret = new ShaderReflection();

  if(dxbc->m_DebugInfo)
  {
    ret->DebugInfo.compileFlags = dxbc->m_DebugInfo->GetShaderCompileFlags();

    create_array_uninit(ret->DebugInfo.files, dxbc->m_DebugInfo->Files.size());
    for(size_t i = 0; i < dxbc->m_DebugInfo->Files.size(); i++)
    {
      ret->DebugInfo.files[i].first = dxbc->m_DebugInfo->Files[i].first;
      ret->DebugInfo.files[i].second = dxbc->m_DebugInfo->Files[i].second;
    }

    string entry = dxbc->m_DebugInfo->GetEntryFunction();
    if(entry.empty())
      entry = "main";

    // sort the file with the entry point to the start. We don't have to do anything if there's only
    // one file.
    // This isn't a perfect search - it will match entry_point() anywhere in the file, even if it's
    // in a comment or disabled preprocessor definition. This is just best-effort
    if(ret->DebugInfo.files.count > 1)
    {
      // search from 0 up. If we find a match, we swap it into [0]. If we don't find a match then
      // we can't rearrange anything. This is a no-op for 0 since it's already in first place, but
      // since our search isn't perfect we might have multiple matches with some being false
      // positives, and so we want to bias towards leaving [0] in place.
      for(int32_t i = 0; i < ret->DebugInfo.files.count; i++)
      {
        char *c = strstr(ret->DebugInfo.files[i].first.elems, entry.c_str());
        char *end = ret->DebugInfo.files[i].first.elems + ret->DebugInfo.files[i].first.count;

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
            std::swap(ret->DebugInfo.files[0], ret->DebugInfo.files[i]);

          break;
        }
      }
    }
  }

  if(dxbc->m_ShaderBlob.empty())
    create_array_uninit(ret->RawBytes, 0);
  else
    create_array_init(ret->RawBytes, dxbc->m_ShaderBlob.size(), &dxbc->m_ShaderBlob[0]);

  ret->DispatchThreadsDimension[0] = dxbc->DispatchThreadsDimension[0];
  ret->DispatchThreadsDimension[1] = dxbc->DispatchThreadsDimension[1];
  ret->DispatchThreadsDimension[2] = dxbc->DispatchThreadsDimension[2];

  ret->InputSig = dxbc->m_InputSig;
  ret->OutputSig = dxbc->m_OutputSig;

  create_array_uninit(ret->ConstantBlocks, dxbc->m_CBuffers.size());
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    ConstantBlock &cb = ret->ConstantBlocks[i];
    cb.name = dxbc->m_CBuffers[i].name;
    cb.bufferBacked = dxbc->m_CBuffers[i].descriptor.type == DXBC::CBuffer::Descriptor::TYPE_CBUFFER;
    cb.byteSize = dxbc->m_CBuffers[i].descriptor.byteSize;
    cb.bindPoint = dxbc->m_CBuffers[i].reg;

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
      bool IsReadOnly = (r.type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
                         r.type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
                         r.type == DXBC::ShaderInputBind::TYPE_SAMPLER ||
                         r.type == DXBC::ShaderInputBind::TYPE_STRUCTURED ||
                         r.type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS);

      if(IsReadOnly)
        numROResources++;
      else
        numRWResources++;
    }
  }

  create_array_uninit(ret->ReadWriteResources, numRWResources);
  create_array_uninit(ret->ReadOnlyResources, numROResources);

  int32_t rwidx = 0, roidx = 0;
  for(size_t i = 0; i < dxbc->m_Resources.size(); i++)
  {
    const auto &r = dxbc->m_Resources[i];

    if(r.type == DXBC::ShaderInputBind::TYPE_CBUFFER)
      continue;

    ShaderResource res;
    res.bindPoint = r.reg;
    res.name = r.name;

    res.IsSampler = (r.type == DXBC::ShaderInputBind::TYPE_SAMPLER);
    res.IsTexture = (r.type == DXBC::ShaderInputBind::TYPE_TEXTURE &&
                     r.dimension != DXBC::ShaderInputBind::DIM_UNKNOWN &&
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFER &&
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFEREX);
    res.IsReadOnly = (r.type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
                      r.type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
                      r.type == DXBC::ShaderInputBind::TYPE_SAMPLER ||
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

    if(res.IsReadOnly)
      ret->ReadOnlyResources[roidx++] = res;
    else
      ret->ReadWriteResources[rwidx++] = res;
  }

  uint32_t numInterfaces = 0;
  for(size_t i = 0; i < dxbc->m_Interfaces.variables.size(); i++)
    numInterfaces = RDCMAX(dxbc->m_Interfaces.variables[i].descriptor.offset + 1, numInterfaces);

  create_array(ret->Interfaces, numInterfaces);
  for(size_t i = 0; i < dxbc->m_Interfaces.variables.size(); i++)
    ret->Interfaces[dxbc->m_Interfaces.variables[i].descriptor.offset] =
        dxbc->m_Interfaces.variables[i].name;

  return ret;
}

/////////////////////////////////////////////////////////////
// Structures/descriptors. Serialise members separately
// instead of ToStrInternal separately. Mostly for convenience of
// debugging the output

template <>
void Serialiser::Serialise(const char *name, D3D11_BUFFER_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_BUFFER_DESC", 0, true);
  Serialise("ByteWidth", el.ByteWidth);
  Serialise("Usage", el.Usage);
  Serialise("BindFlags", (D3D11_BIND_FLAG &)el.BindFlags);
  Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG &)el.CPUAccessFlags);
  Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG &)el.MiscFlags);
  Serialise("StructureByteStride", el.StructureByteStride);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE1D_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_TEXTURE1D_DESC", 0, true);
  Serialise("Width", el.Width);
  Serialise("MipLevels", el.MipLevels);
  Serialise("ArraySize", el.ArraySize);
  Serialise("Format", el.Format);
  Serialise("Usage", el.Usage);
  Serialise("BindFlags", (D3D11_BIND_FLAG &)el.BindFlags);
  Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG &)el.CPUAccessFlags);
  Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG &)el.MiscFlags);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE2D_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_TEXTURE2D_DESC", 0, true);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("MipLevels", el.MipLevels);
  Serialise("ArraySize", el.ArraySize);
  Serialise("Format", el.Format);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("Usage", el.Usage);
  Serialise("BindFlags", (D3D11_BIND_FLAG &)el.BindFlags);
  Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG &)el.CPUAccessFlags);
  Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG &)el.MiscFlags);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE2D_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_TEXTURE2D_DESC1", 0, true);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("MipLevels", el.MipLevels);
  Serialise("ArraySize", el.ArraySize);
  Serialise("Format", el.Format);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("Usage", el.Usage);
  Serialise("BindFlags", (D3D11_BIND_FLAG &)el.BindFlags);
  Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG &)el.CPUAccessFlags);
  Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG &)el.MiscFlags);
  Serialise("TextureLayout", (D3D11_TEXTURE_LAYOUT &)el.TextureLayout);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE3D_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_TEXTURE3D_DESC", 0, true);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("Depth", el.Depth);
  Serialise("MipLevels", el.MipLevels);
  Serialise("Format", el.Format);
  Serialise("Usage", el.Usage);
  Serialise("BindFlags", (D3D11_BIND_FLAG &)el.BindFlags);
  Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG &)el.CPUAccessFlags);
  Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG &)el.MiscFlags);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE3D_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_TEXTURE3D_DESC1", 0, true);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("Depth", el.Depth);
  Serialise("MipLevels", el.MipLevels);
  Serialise("Format", el.Format);
  Serialise("Usage", el.Usage);
  Serialise("BindFlags", (D3D11_BIND_FLAG &)el.BindFlags);
  Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG &)el.CPUAccessFlags);
  Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG &)el.MiscFlags);
  Serialise("TextureLayout", (D3D11_TEXTURE_LAYOUT &)el.TextureLayout);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_SHADER_RESOURCE_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_SHADER_RESOURCE_VIEW_DESC", 0, true);
  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipLevels", el.Texture1D.MipLevels);
      Serialise("Texture1D.MostDetailedMip", el.Texture1D.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipLevels", el.Texture1DArray.MipLevels);
      Serialise("Texture1DArray.MostDetailedMip", el.Texture1DArray.MostDetailedMip);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipLevels", el.Texture2D.MipLevels);
      Serialise("Texture2D.MostDetailedMip", el.Texture2D.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipLevels", el.Texture2DArray.MipLevels);
      Serialise("Texture2DArray.MostDetailedMip", el.Texture2DArray.MostDetailedMip);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipLevels", el.Texture3D.MipLevels);
      Serialise("Texture3D.MostDetailedMip", el.Texture3D.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE:
      Serialise("TextureCube.MipLevels", el.TextureCube.MipLevels);
      Serialise("TextureCube.MostDetailedMip", el.TextureCube.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
      Serialise("TextureCubeArray.MipLevels", el.TextureCubeArray.MipLevels);
      Serialise("TextureCubeArray.MostDetailedMip", el.TextureCubeArray.MostDetailedMip);
      Serialise("TextureCubeArray.NumCubes", el.TextureCubeArray.NumCubes);
      Serialise("TextureCubeArray.First2DArrayFace", el.TextureCubeArray.First2DArrayFace);
      break;
    case D3D11_SRV_DIMENSION_BUFFEREX:
      Serialise("Buffer.FirstElement", el.BufferEx.FirstElement);
      Serialise("Buffer.NumElements", el.BufferEx.NumElements);
      Serialise("Buffer.Flags", el.BufferEx.Flags);
      break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_SHADER_RESOURCE_VIEW_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_SHADER_RESOURCE_VIEW_DESC1", 0, true);
  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipLevels", el.Texture1D.MipLevels);
      Serialise("Texture1D.MostDetailedMip", el.Texture1D.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipLevels", el.Texture1DArray.MipLevels);
      Serialise("Texture1DArray.MostDetailedMip", el.Texture1DArray.MostDetailedMip);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipLevels", el.Texture2D.MipLevels);
      Serialise("Texture2D.MostDetailedMip", el.Texture2D.MostDetailedMip);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipLevels", el.Texture2DArray.MipLevels);
      Serialise("Texture2DArray.MostDetailedMip", el.Texture2DArray.MostDetailedMip);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      break;
    case D3D11_SRV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipLevels", el.Texture3D.MipLevels);
      Serialise("Texture3D.MostDetailedMip", el.Texture3D.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE:
      Serialise("TextureCube.MipLevels", el.TextureCube.MipLevels);
      Serialise("TextureCube.MostDetailedMip", el.TextureCube.MostDetailedMip);
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
      Serialise("TextureCubeArray.MipLevels", el.TextureCubeArray.MipLevels);
      Serialise("TextureCubeArray.MostDetailedMip", el.TextureCubeArray.MostDetailedMip);
      Serialise("TextureCubeArray.NumCubes", el.TextureCubeArray.NumCubes);
      Serialise("TextureCubeArray.First2DArrayFace", el.TextureCubeArray.First2DArrayFace);
      break;
    case D3D11_SRV_DIMENSION_BUFFEREX:
      Serialise("Buffer.FirstElement", el.BufferEx.FirstElement);
      Serialise("Buffer.NumElements", el.BufferEx.NumElements);
      Serialise("Buffer.Flags", el.BufferEx.Flags);
      break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_RENDER_TARGET_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_RENDER_TARGET_VIEW_DESC", 0, true);
  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_RENDER_TARGET_VIEW_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_RENDER_TARGET_VIEW_DESC1", 0, true);
  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      break;
    case D3D11_RTV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_UNORDERED_ACCESS_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_UNORDERED_ACCESS_VIEW_DESC", 0, true);
  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      Serialise("Buffer.Flags", el.Buffer.Flags);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_UNORDERED_ACCESS_VIEW_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_UNORDERED_ACCESS_VIEW_DESC1", 0, true);
  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      Serialise("Buffer.Flags", el.Buffer.Flags);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_DEPTH_STENCIL_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_DEPTH_STENCIL_VIEW_DESC", 0, true);
  Serialise("Format", el.Format);
  Serialise("Flags", el.Flags);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      break;
    default: RDCERR("Unrecognised DSV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_BLEND_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_BLEND_DESC", 0, true);

  Serialise("AlphaToCoverageEnable", el.AlphaToCoverageEnable);
  Serialise("IndependentBlendEnable", el.IndependentBlendEnable);
  for(int i = 0; i < 8; i++)
  {
    ScopedContext targetscope(this, name, "D3D11_RENDER_TARGET_BLEND_DESC", 0, true);

    bool enable = el.RenderTarget[i].BlendEnable == TRUE;
    Serialise("BlendEnable", enable);
    el.RenderTarget[i].BlendEnable = enable;

    {
      Serialise("SrcBlend", el.RenderTarget[i].SrcBlend);
      Serialise("DestBlend", el.RenderTarget[i].DestBlend);
      Serialise("BlendOp", el.RenderTarget[i].BlendOp);
      Serialise("SrcBlendAlpha", el.RenderTarget[i].SrcBlendAlpha);
      Serialise("DestBlendAlpha", el.RenderTarget[i].DestBlendAlpha);
      Serialise("BlendOpAlpha", el.RenderTarget[i].BlendOpAlpha);
    }

    Serialise("RenderTargetWriteMask", el.RenderTarget[i].RenderTargetWriteMask);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_BLEND_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_BLEND_DESC1", 0, true);

  Serialise("AlphaToCoverageEnable", el.AlphaToCoverageEnable);
  Serialise("IndependentBlendEnable", el.IndependentBlendEnable);
  for(int i = 0; i < 8; i++)
  {
    ScopedContext targetscope(this, name, "D3D11_RENDER_TARGET_BLEND_DESC1", 0, true);

    bool enable = el.RenderTarget[i].BlendEnable == TRUE;
    Serialise("BlendEnable", enable);
    el.RenderTarget[i].BlendEnable = enable;

    enable = el.RenderTarget[i].LogicOpEnable == TRUE;
    Serialise("LogicOpEnable", enable);
    el.RenderTarget[i].LogicOpEnable = enable;

    {
      Serialise("SrcBlend", el.RenderTarget[i].SrcBlend);
      Serialise("DestBlend", el.RenderTarget[i].DestBlend);
      Serialise("BlendOp", el.RenderTarget[i].BlendOp);
      Serialise("SrcBlendAlpha", el.RenderTarget[i].SrcBlendAlpha);
      Serialise("DestBlendAlpha", el.RenderTarget[i].DestBlendAlpha);
      Serialise("BlendOpAlpha", el.RenderTarget[i].BlendOpAlpha);
      Serialise("LogicOp", el.RenderTarget[i].LogicOp);
    }

    Serialise("RenderTargetWriteMask", el.RenderTarget[i].RenderTargetWriteMask);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_DEPTH_STENCIL_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_DEPTH_STENCIL_DESC", 0, true);

  Serialise("DepthEnable", el.DepthEnable);
  Serialise("DepthWriteMask", el.DepthWriteMask);
  Serialise("DepthFunc", el.DepthFunc);
  Serialise("StencilEnable", el.StencilEnable);
  Serialise("StencilReadMask", el.StencilReadMask);
  Serialise("StencilWriteMask", el.StencilWriteMask);

  {
    ScopedContext opscope(this, name, "D3D11_DEPTH_STENCILOP_DESC", 0, true);
    Serialise("FrontFace.StencilFailOp", el.FrontFace.StencilFailOp);
    Serialise("FrontFace.StencilDepthFailOp", el.FrontFace.StencilDepthFailOp);
    Serialise("FrontFace.StencilPassOp", el.FrontFace.StencilPassOp);
    Serialise("FrontFace.StencilFunc", el.FrontFace.StencilFunc);
  }
  {
    ScopedContext opscope(this, name, "D3D11_DEPTH_STENCILOP_DESC", 0, true);
    Serialise("BackFace.StencilFailOp", el.BackFace.StencilFailOp);
    Serialise("BackFace.StencilDepthFailOp", el.BackFace.StencilDepthFailOp);
    Serialise("BackFace.StencilPassOp", el.BackFace.StencilPassOp);
    Serialise("BackFace.StencilFunc", el.BackFace.StencilFunc);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_RASTERIZER_DESC", 0, true);

  Serialise("FillMode", el.FillMode);
  Serialise("CullMode", el.CullMode);
  Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
  Serialise("DepthBias", el.DepthBias);
  Serialise("DepthBiasClamp", el.DepthBiasClamp);
  Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
  Serialise("DepthClipEnable", el.DepthClipEnable);
  Serialise("ScissorEnable", el.ScissorEnable);
  Serialise("MultisampleEnable", el.MultisampleEnable);
  Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_RASTERIZER_DESC1", 0, true);

  Serialise("FillMode", el.FillMode);
  Serialise("CullMode", el.CullMode);
  Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
  Serialise("DepthBias", el.DepthBias);
  Serialise("DepthBiasClamp", el.DepthBiasClamp);
  Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
  Serialise("DepthClipEnable", el.DepthClipEnable);
  Serialise("ScissorEnable", el.ScissorEnable);
  Serialise("MultisampleEnable", el.MultisampleEnable);
  Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
  Serialise("ForcedSampleCount", el.ForcedSampleCount);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC2 &el)
{
  ScopedContext scope(this, name, "D3D11_RASTERIZER_DESC2", 0, true);

  Serialise("FillMode", el.FillMode);
  Serialise("CullMode", el.CullMode);
  Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
  Serialise("DepthBias", el.DepthBias);
  Serialise("DepthBiasClamp", el.DepthBiasClamp);
  Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
  Serialise("DepthClipEnable", el.DepthClipEnable);
  Serialise("ScissorEnable", el.ScissorEnable);
  Serialise("MultisampleEnable", el.MultisampleEnable);
  Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
  Serialise("ForcedSampleCount", el.ForcedSampleCount);
  Serialise("ConservativeRaster", (D3D11_CONSERVATIVE_RASTERIZATION_MODE &)el.ConservativeRaster);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_QUERY_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_QUERY_DESC", 0, true);

  Serialise("MiscFlags", el.MiscFlags);
  Serialise("Query", el.Query);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_QUERY_DESC1 &el)
{
  ScopedContext scope(this, name, "D3D11_QUERY_DESC1", 0, true);

  Serialise("MiscFlags", el.MiscFlags);
  Serialise("Query", el.Query);
  Serialise("ContextType", (D3D11_CONTEXT_TYPE &)el.ContextType);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_COUNTER_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_COUNTER_DESC", 0, true);

  Serialise("MiscFlags", el.MiscFlags);
  Serialise("Counter", el.Counter);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_SAMPLER_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_SAMPLER_DESC", 0, true);

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
void Serialiser::Serialise(const char *name, D3D11_SO_DECLARATION_ENTRY &el)
{
  ScopedContext scope(this, name, "D3D11_SO_DECLARATION_ENTRY", 0, true);

  string s = "";
  if(m_Mode >= WRITING && el.SemanticName != NULL)
    s = el.SemanticName;

  Serialise("SemanticName", s);

  if(m_Mode == READING)
  {
    if(s == "")
    {
      el.SemanticName = NULL;
    }
    else
    {
      string str = (char *)m_BufferHead - s.length();
      m_StringDB.insert(str);
      el.SemanticName = m_StringDB.find(str)->c_str();
    }
  }

  // so we can just take a char* into the buffer above for the semantic name,
  // ensure we serialise a null terminator (slightly redundant because the above
  // serialise of the string wrote out the length, but not the end of the world).
  char nullterminator = 0;
  Serialise(NULL, nullterminator);

  Serialise("SemanticIndex", el.SemanticIndex);
  Serialise("Stream", el.Stream);
  Serialise("StartComponent", el.StartComponent);
  Serialise("ComponentCount", el.ComponentCount);
  Serialise("OutputSlot", el.OutputSlot);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_INPUT_ELEMENT_DESC &el)
{
  ScopedContext scope(this, name, "D3D11_INPUT_ELEMENT_DESC", 0, true);

  string s;
  if(m_Mode >= WRITING)
    s = el.SemanticName;

  Serialise("SemanticName", s);

  if(m_Mode == READING)
  {
    string str = (char *)m_BufferHead - s.length();
    m_StringDB.insert(str);
    el.SemanticName = m_StringDB.find(str)->c_str();
  }

  // so we can just take a char* into the buffer above for the semantic name,
  // ensure we serialise a null terminator (slightly redundant because the above
  // serialise of the string wrote out the length, but not the end of the world).
  char nullterminator = 0;
  Serialise(NULL, nullterminator);

  Serialise("SemanticIndex", el.SemanticIndex);
  Serialise("Format", el.Format);
  Serialise("InputSlot", el.InputSlot);
  Serialise("AlignedByteOffset", el.AlignedByteOffset);
  Serialise("InputSlotClass", el.InputSlotClass);
  Serialise("InstanceDataStepRate", el.InstanceDataStepRate);
}

template <>
void Serialiser::Serialise(const char *name, D3D11_SUBRESOURCE_DATA &el)
{
  ScopedContext scope(this, name, "D3D11_SUBRESOURCE_DATA", 0, true);

  // el.pSysMem
  Serialise("SysMemPitch", el.SysMemPitch);
  Serialise("SysMemSlicePitch", el.SysMemSlicePitch);
}

/////////////////////////////////////////////////////////////
// Trivial structures

string ToStrHelper<false, D3D11_VIEWPORT>::Get(const D3D11_VIEWPORT &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "Viewport<%.0fx%.0f+%.0f+%.0f z=%f->%f>", el.Width,
                         el.Height, el.TopLeftX, el.TopLeftY, el.MinDepth, el.MaxDepth);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_RECT>::Get(const D3D11_RECT &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "RECT<%d,%d,%d,%d>", el.left, el.right, el.top, el.bottom);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_BOX>::Get(const D3D11_BOX &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "BOX<%d,%d,%d,%d,%d,%d>", el.left, el.right, el.top,
                         el.bottom, el.front, el.back);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_BIND_FLAG>::Get(const D3D11_BIND_FLAG &el)
{
  string ret;

  if(el & D3D11_BIND_VERTEX_BUFFER)
    ret += " | D3D11_BIND_VERTEX_BUFFER";
  if(el & D3D11_BIND_INDEX_BUFFER)
    ret += " | D3D11_BIND_INDEX_BUFFER";
  if(el & D3D11_BIND_CONSTANT_BUFFER)
    ret += " | D3D11_BIND_CONSTANT_BUFFER";
  if(el & D3D11_BIND_SHADER_RESOURCE)
    ret += " | D3D11_BIND_SHADER_RESOURCE";
  if(el & D3D11_BIND_STREAM_OUTPUT)
    ret += " | D3D11_BIND_STREAM_OUTPUT";
  if(el & D3D11_BIND_RENDER_TARGET)
    ret += " | D3D11_BIND_RENDER_TARGET";
  if(el & D3D11_BIND_DEPTH_STENCIL)
    ret += " | D3D11_BIND_DEPTH_STENCIL";
  if(el & D3D11_BIND_UNORDERED_ACCESS)
    ret += " | D3D11_BIND_UNORDERED_ACCESS";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D11_CPU_ACCESS_FLAG>::Get(const D3D11_CPU_ACCESS_FLAG &el)
{
  string ret;

  if(el & D3D11_CPU_ACCESS_READ)
    ret += " | D3D11_CPU_ACCESS_READ";
  if(el & D3D11_CPU_ACCESS_WRITE)
    ret += " | D3D11_CPU_ACCESS_WRITE";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D11_RESOURCE_MISC_FLAG>::Get(const D3D11_RESOURCE_MISC_FLAG &el)
{
  string ret;

  if(el & D3D11_RESOURCE_MISC_GENERATE_MIPS)
    ret + " | D3D11_RESOURCE_MISC_GENERATE_MIPS";
  if(el & D3D11_RESOURCE_MISC_SHARED)
    ret + " | D3D11_RESOURCE_MISC_SHARED";
  if(el & D3D11_RESOURCE_MISC_TEXTURECUBE)
    ret + " | D3D11_RESOURCE_MISC_TEXTURECUBE";
  if(el & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)
    ret + " | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS";
  if(el & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    ret + " | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS";
  if(el & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    ret + " | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED";
  if(el & D3D11_RESOURCE_MISC_RESOURCE_CLAMP)
    ret + " | D3D11_RESOURCE_MISC_RESOURCE_CLAMP";
  if(el & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
    ret + " | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX";
  if(el & D3D11_RESOURCE_MISC_GDI_COMPATIBLE)
    ret + " | D3D11_RESOURCE_MISC_GDI_COMPATIBLE";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D11_TEXTURE_LAYOUT>::Get(const D3D11_TEXTURE_LAYOUT &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_TEXTURE_LAYOUT_UNDEFINED)
    TOSTR_CASE_STRINGIZE(D3D11_TEXTURE_LAYOUT_ROW_MAJOR)
    TOSTR_CASE_STRINGIZE(D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE)
    default: break;
  }

  return StringFormat::Fmt("D3D11_TEXTURE_LAYOUT<%d>", el);
}

/////////////////////////////////////////////////////////////
// Enums and lists

string ToStrHelper<false, D3D11_DEPTH_WRITE_MASK>::Get(const D3D11_DEPTH_WRITE_MASK &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_DEPTH_WRITE_MASK_ZERO)
    TOSTR_CASE_STRINGIZE(D3D11_DEPTH_WRITE_MASK_ALL)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_DEPTH_WRITE_MASK<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_COMPARISON_FUNC>::Get(const D3D11_COMPARISON_FUNC &el)
{
  switch(el)
  {
    case D3D11_COMPARISON_NEVER: return "NEVER";
    case D3D11_COMPARISON_LESS: return "LESS";
    case D3D11_COMPARISON_EQUAL: return "EQUAL";
    case D3D11_COMPARISON_LESS_EQUAL: return "LESS_EQUAL";
    case D3D11_COMPARISON_GREATER: return "GREATER";
    case D3D11_COMPARISON_NOT_EQUAL: return "NOT_EQUAL";
    case D3D11_COMPARISON_GREATER_EQUAL: return "GREATER_EQUAL";
    case D3D11_COMPARISON_ALWAYS: return "ALWAYS";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_COMPARISON_FUNC<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_STENCIL_OP>::Get(const D3D11_STENCIL_OP &el)
{
  switch(el)
  {
    case D3D11_STENCIL_OP_KEEP: return "KEEP";
    case D3D11_STENCIL_OP_ZERO: return "ZERO";
    case D3D11_STENCIL_OP_REPLACE: return "REPLACE";
    case D3D11_STENCIL_OP_INCR_SAT: return "INCR_SAT";
    case D3D11_STENCIL_OP_DECR_SAT: return "DECR_SAT";
    case D3D11_STENCIL_OP_INVERT: return "INVERT";
    case D3D11_STENCIL_OP_INCR: return "INCR";
    case D3D11_STENCIL_OP_DECR: return "DECR";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_STENCIL_OP<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_BLEND>::Get(const D3D11_BLEND &el)
{
  switch(el)
  {
    case D3D11_BLEND_ZERO: return "ZERO";
    case D3D11_BLEND_ONE: return "ONE";
    case D3D11_BLEND_SRC_COLOR: return "SRC_COLOR";
    case D3D11_BLEND_INV_SRC_COLOR: return "INV_SRC_COLOR";
    case D3D11_BLEND_SRC_ALPHA: return "SRC_ALPHA";
    case D3D11_BLEND_INV_SRC_ALPHA: return "INV_SRC_ALPHA";
    case D3D11_BLEND_DEST_ALPHA: return "DEST_ALPHA";
    case D3D11_BLEND_INV_DEST_ALPHA: return "INV_DEST_ALPHA";
    case D3D11_BLEND_DEST_COLOR: return "DEST_COLOR";
    case D3D11_BLEND_INV_DEST_COLOR: return "INV_DEST_COLOR";
    case D3D11_BLEND_SRC_ALPHA_SAT: return "SRC_ALPHA_SAT";
    case D3D11_BLEND_BLEND_FACTOR: return "BLEND_FACTOR";
    case D3D11_BLEND_INV_BLEND_FACTOR: return "INV_BLEND_FACTOR";
    case D3D11_BLEND_SRC1_COLOR: return "SRC1_COLOR";
    case D3D11_BLEND_INV_SRC1_COLOR: return "INV_SRC1_COLOR";
    case D3D11_BLEND_SRC1_ALPHA: return "SRC1_ALPHA";
    case D3D11_BLEND_INV_SRC1_ALPHA: return "INV_SRC1_ALPHA";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_BLEND<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_BLEND_OP>::Get(const D3D11_BLEND_OP &el)
{
  switch(el)
  {
    case D3D11_BLEND_OP_ADD: return "ADD";
    case D3D11_BLEND_OP_SUBTRACT: return "SUBTRACT";
    case D3D11_BLEND_OP_REV_SUBTRACT: return "REV_SUBTRACT";
    case D3D11_BLEND_OP_MIN: return "MIN";
    case D3D11_BLEND_OP_MAX: return "MAX";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_BLEND_OP<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_CULL_MODE>::Get(const D3D11_CULL_MODE &el)
{
  switch(el)
  {
    case D3D11_CULL_NONE: return "NONE";
    case D3D11_CULL_FRONT: return "FRONT";
    case D3D11_CULL_BACK: return "BACK";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_CULL_MODE<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_FILL_MODE>::Get(const D3D11_FILL_MODE &el)
{
  switch(el)
  {
    case D3D11_FILL_WIREFRAME: return "WIREFRAME";
    case D3D11_FILL_SOLID: return "SOLID";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_FILL_MODE<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_CONSERVATIVE_RASTERIZATION_MODE>::Get(
    const D3D11_CONSERVATIVE_RASTERIZATION_MODE &el)
{
  return el == D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON ? "ON" : "OFF";
}

string ToStrHelper<false, D3D11_TEXTURE_ADDRESS_MODE>::Get(const D3D11_TEXTURE_ADDRESS_MODE &el)
{
  switch(el)
  {
    case D3D11_TEXTURE_ADDRESS_WRAP: return "WRAP";
    case D3D11_TEXTURE_ADDRESS_MIRROR: return "MIRROR";
    case D3D11_TEXTURE_ADDRESS_CLAMP: return "CLAMP";
    case D3D11_TEXTURE_ADDRESS_BORDER: return "BORDER";
    case D3D11_TEXTURE_ADDRESS_MIRROR_ONCE: return "MIRROR_ONCE";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_TEXTURE_ADDRESS_MODE<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_FILTER>::Get(const D3D11_FILTER &el)
{
  switch(el)
  {
    case D3D11_FILTER_MIN_MAG_MIP_POINT: return "MIN_MAG_MIP_POINT";
    case D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR: return "MIN_MAG_POINT_MIP_LINEAR";
    case D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT: return "MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR: return "MIN_POINT_MAG_MIP_LINEAR";
    case D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT: return "MIN_LINEAR_MAG_MIP_POINT";
    case D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return "MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT: return "MIN_MAG_LINEAR_MIP_POINT";
    case D3D11_FILTER_MIN_MAG_MIP_LINEAR: return "MIN_MAG_MIP_LINEAR";
    case D3D11_FILTER_ANISOTROPIC: return "ANISOTROPIC";
    case D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT: return "CMP:MIN_MAG_MIP_POINT";
    case D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR: return "CMP:MIN_MAG_POINT_MIP_LINEAR";
    case D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "CMP:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR: return "CMP:MIN_POINT_MAG_MIP_LINEAR";
    case D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT: return "CMP:MIN_LINEAR_MAG_MIP_POINT";
    case D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "CMP:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT: return "CMP:MIN_MAG_LINEAR_MIP_POINT";
    case D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR: return "CMP:MIN_MAG_MIP_LINEAR";
    case D3D11_FILTER_COMPARISON_ANISOTROPIC: return "CMP:ANISOTROPIC";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_FILTER<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_SRV_DIMENSION>::Get(const D3D11_SRV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE3D)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURECUBE)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_BUFFEREX)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_SRV_DIMENSION<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_RTV_DIMENSION>::Get(const D3D11_RTV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE3D)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_RTV_DIMENSION<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_UAV_DIMENSION>::Get(const D3D11_UAV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE3D)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_UAV_DIMENSION<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_DSV_DIMENSION>::Get(const D3D11_DSV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_DSV_DIMENSION<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_CONTEXT_TYPE>::Get(const D3D11_CONTEXT_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_CONTEXT_TYPE_ALL)
    TOSTR_CASE_STRINGIZE(D3D11_CONTEXT_TYPE_3D)
    TOSTR_CASE_STRINGIZE(D3D11_CONTEXT_TYPE_COMPUTE)
    TOSTR_CASE_STRINGIZE(D3D11_CONTEXT_TYPE_COPY)
    TOSTR_CASE_STRINGIZE(D3D11_CONTEXT_TYPE_VIDEO)
    default: break;
  }

  return StringFormat::Fmt("D3D11_CONTEXT_TYPE<%d>", el);
}

string ToStrHelper<false, D3D11_QUERY>::Get(const D3D11_QUERY &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_EVENT)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_OCCLUSION)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_TIMESTAMP)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_TIMESTAMP_DISJOINT)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_PIPELINE_STATISTICS)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_OCCLUSION_PREDICATE)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM0)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM1)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM2)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM3)
    TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_QUERY<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_COUNTER>::Get(const D3D11_COUNTER &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_COUNTER_DEVICE_DEPENDENT_0)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_COUNTER<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_MAP>::Get(const D3D11_MAP &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_MAP_READ)
    TOSTR_CASE_STRINGIZE(D3D11_MAP_WRITE)
    TOSTR_CASE_STRINGIZE(D3D11_MAP_READ_WRITE)
    TOSTR_CASE_STRINGIZE(D3D11_MAP_WRITE_DISCARD)
    TOSTR_CASE_STRINGIZE(D3D11_MAP_WRITE_NO_OVERWRITE)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_MAP<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_PRIMITIVE_TOPOLOGY>::Get(const D3D11_PRIMITIVE_TOPOLOGY &el)
{
  switch(el)
  {
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST: return "PointList";
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST: return "LineList";
    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP: return "LineStrip";
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST: return "TriangleList";
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return "TriangleStrip";
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: return "LineListAdj";
    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ: return "LineStripAdj";
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ: return "TriangleListAdj";
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ: return "TriangleStripAdj";
    default: break;
  }

  char tostrBuf[256] = {0};

  if(el >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST &&
     el <= D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST)
  {
    StringFormat::snprintf(tostrBuf, 255, "Patchlist_%dCPs",
                           el - D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1);
  }
  else
  {
    StringFormat::snprintf(tostrBuf, 255, "D3D11_PRIMITIVE_TOPOLOGY<%d>", el);
  }

  return tostrBuf;
}

string ToStrHelper<false, D3D11_USAGE>::Get(const D3D11_USAGE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_USAGE_DEFAULT)
    TOSTR_CASE_STRINGIZE(D3D11_USAGE_IMMUTABLE)
    TOSTR_CASE_STRINGIZE(D3D11_USAGE_DYNAMIC)
    TOSTR_CASE_STRINGIZE(D3D11_USAGE_STAGING)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_USAGE<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_INPUT_CLASSIFICATION>::Get(const D3D11_INPUT_CLASSIFICATION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D11_INPUT_PER_VERTEX_DATA)
    TOSTR_CASE_STRINGIZE(D3D11_INPUT_PER_INSTANCE_DATA)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_INPUT_CLASSIFICATION<%d>", el);

  return tostrBuf;
}

string ToStrHelper<false, D3D11_LOGIC_OP>::Get(const D3D11_LOGIC_OP &el)
{
  switch(el)
  {
    case D3D11_LOGIC_OP_CLEAR: return "CLEAR";
    case D3D11_LOGIC_OP_SET: return "SET";
    case D3D11_LOGIC_OP_COPY: return "COPY";
    case D3D11_LOGIC_OP_COPY_INVERTED: return "COPY_INVERTED";
    case D3D11_LOGIC_OP_NOOP: return "NOOP";
    case D3D11_LOGIC_OP_INVERT: return "INVERT";
    case D3D11_LOGIC_OP_AND: return "AND";
    case D3D11_LOGIC_OP_NAND: return "NAND";
    case D3D11_LOGIC_OP_OR: return "OR";
    case D3D11_LOGIC_OP_NOR: return "NOR";
    case D3D11_LOGIC_OP_XOR: return "XOR";
    case D3D11_LOGIC_OP_EQUIV: return "EQUIV";
    case D3D11_LOGIC_OP_AND_REVERSE: return "AND_REVERSE";
    case D3D11_LOGIC_OP_AND_INVERTED: return "AND_INVERTED";
    case D3D11_LOGIC_OP_OR_REVERSE: return "OR_REVERSE";
    case D3D11_LOGIC_OP_OR_INVERTED: return "OR_INVERTED";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "D3D11_LOGIC_OP<%d>", el);

  return tostrBuf;
}

// for HRESULT
template <>
string ToStrHelper<false, long>::Get(const long &el)
{
  return ToStr::Get((uint64_t)el);
}
