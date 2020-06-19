/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "dxbc_reflect.h"
#include "common/formatting.h"
#include "core/core.h"
#include "dxbc_container.h"

static ShaderConstant MakeConstantBufferVariable(const DXBC::CBufferVariable &var);

static ShaderVariableType MakeShaderVariableType(DXBC::CBufferVariableType type)
{
  ShaderVariableType ret;

  ret.descriptor.type = type.descriptor.varType;
  ret.descriptor.rows = (uint8_t)type.descriptor.rows;
  ret.descriptor.columns = (uint8_t)type.descriptor.cols;
  ret.descriptor.elements = type.descriptor.elements;
  ret.descriptor.name = type.descriptor.name;
  ret.descriptor.rowMajorStorage = (type.descriptor.varClass == DXBC::CLASS_MATRIX_ROWS ||
                                    type.descriptor.varClass == DXBC::CLASS_VECTOR ||
                                    type.descriptor.varClass == DXBC::CLASS_SCALAR);

  uint32_t baseElemSize = (ret.descriptor.type == VarType::Double) ? 8 : 4;

  // in D3D matrices always take up a float4 per row/column
  ret.descriptor.matrixByteStride = uint8_t(baseElemSize * 4);

  if(type.descriptor.varClass == DXBC::CLASS_STRUCT)
  {
    uint32_t stride = type.descriptor.bytesize / RDCMAX(1U, type.descriptor.elements);
    RDCASSERTMSG("Stride is too large for uint16_t", stride <= 0xffff);
    ret.descriptor.arrayByteStride = AlignUp16(RDCMIN(stride, 0xffffu) & 0xffff);

    ret.descriptor.rows = ret.descriptor.columns = 0;
  }
  else
  {
    if(ret.descriptor.rowMajorStorage)
      ret.descriptor.arrayByteStride = ret.descriptor.matrixByteStride * ret.descriptor.rows;
    else
      ret.descriptor.arrayByteStride = ret.descriptor.matrixByteStride * ret.descriptor.columns;
  }

  ret.members.reserve(type.members.size());
  for(size_t i = 0; i < type.members.size(); i++)
    ret.members.push_back(MakeConstantBufferVariable(type.members[i]));

  if(!ret.members.empty())
  {
    ret.descriptor.rows = 0;
    ret.descriptor.columns = 0;
  }

  return ret;
}

static ShaderConstant MakeConstantBufferVariable(const DXBC::CBufferVariable &var)
{
  ShaderConstant ret;

  ret.name = var.name;
  ret.byteOffset = var.offset;
  ret.defaultValue = 0;
  ret.type = MakeShaderVariableType(var.type);

  return ret;
}

static void MakeResourceList(bool srv, DXBC::DXBCContainer *dxbc,
                             const rdcarray<DXBC::ShaderInputBind> &in,
                             rdcarray<Bindpoint> &mapping, rdcarray<ShaderResource> &refl)
{
  for(size_t i = 0; i < in.size(); i++)
  {
    const DXBC::ShaderInputBind &r = in[i];

    ShaderResource res;
    res.name = r.name;

    res.isTexture = ((r.type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
                      r.type == DXBC::ShaderInputBind::TYPE_UAV_RWTYPED) &&
                     r.dimension != DXBC::ShaderInputBind::DIM_UNKNOWN &&
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFER &&
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFEREX);
    res.isReadOnly = srv;

    switch(r.dimension)
    {
      default:
      case DXBC::ShaderInputBind::DIM_UNKNOWN: res.resType = TextureType::Unknown; break;
      case DXBC::ShaderInputBind::DIM_BUFFER:
      case DXBC::ShaderInputBind::DIM_BUFFEREX: res.resType = TextureType::Buffer; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE1D: res.resType = TextureType::Texture1D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY:
        res.resType = TextureType::Texture1DArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2D: res.resType = TextureType::Texture2D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY:
        res.resType = TextureType::Texture2DArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DMS: res.resType = TextureType::Texture2DMS; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY:
        res.resType = TextureType::Texture2DMSArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE3D: res.resType = TextureType::Texture3D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURECUBE: res.resType = TextureType::TextureCube; break;
      case DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY:
        res.resType = TextureType::TextureCubeArray;
        break;
    }

    if(r.type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS ||
       r.type == DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS)
    {
      res.variableType.descriptor.rows = res.variableType.descriptor.columns = 1;
      res.variableType.descriptor.elements = 1;
      res.variableType.descriptor.type = VarType::UByte;
      res.variableType.descriptor.name = "byte";
    }
    else if(r.retType != DXBC::RETURN_TYPE_UNKNOWN && r.retType != DXBC::RETURN_TYPE_MIXED &&
            r.retType != DXBC::RETURN_TYPE_CONTINUED)
    {
      res.variableType.descriptor.rows = 1;
      res.variableType.descriptor.columns = (uint8_t)r.numComps;
      res.variableType.descriptor.elements = 1;

      rdcstr name;

      switch(r.retType)
      {
        case DXBC::RETURN_TYPE_UNORM:
          name = "unorm float";
          res.variableType.descriptor.type = VarType::Float;
          break;
        case DXBC::RETURN_TYPE_SNORM:
          name = "snorm float";
          res.variableType.descriptor.type = VarType::Float;
          break;
        case DXBC::RETURN_TYPE_SINT:
          name = "int";
          res.variableType.descriptor.type = VarType::SInt;
          break;
        case DXBC::RETURN_TYPE_UINT:
          name = "uint";
          res.variableType.descriptor.type = VarType::UInt;
          break;
        case DXBC::RETURN_TYPE_FLOAT:
          name = "float";
          res.variableType.descriptor.type = VarType::Float;
          break;
        case DXBC::RETURN_TYPE_DOUBLE:
          name = "double";
          res.variableType.descriptor.type = VarType::Double;
          break;
        default: name = "unknown"; break;
      }

      if(r.numComps > 1)
        name += StringFormat::Fmt("%u", r.numComps);

      res.variableType.descriptor.name = name;
    }
    else
    {
      auto it = dxbc->GetReflection()->ResourceBinds.find(r.name);
      if(it != dxbc->GetReflection()->ResourceBinds.end())
      {
        res.variableType = MakeShaderVariableType(it->second);
      }
      else
      {
        res.variableType.descriptor.rows = 0;
        res.variableType.descriptor.columns = 0;
        res.variableType.descriptor.elements = 0;
        res.variableType.descriptor.name = "";
      }
    }

    res.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = r.bindCount == 0 ? ~0U : r.bindCount;
    map.bindset = r.space;
    map.bind = r.reg;
    map.used = true;

    mapping[i] = map;
    refl[i] = res;
  }
}

void MakeShaderReflection(DXBC::DXBCContainer *dxbc, ShaderReflection *refl,
                          ShaderBindpointMapping *mapping)
{
  if(dxbc == NULL || !RenderDoc::Inst().IsReplayApp())
    return;

  switch(dxbc->m_Type)
  {
    case DXBC::ShaderType::Pixel: refl->stage = ShaderStage::Pixel; break;
    case DXBC::ShaderType::Vertex: refl->stage = ShaderStage::Vertex; break;
    case DXBC::ShaderType::Geometry: refl->stage = ShaderStage::Geometry; break;
    case DXBC::ShaderType::Hull: refl->stage = ShaderStage::Hull; break;
    case DXBC::ShaderType::Domain: refl->stage = ShaderStage::Domain; break;
    case DXBC::ShaderType::Compute: refl->stage = ShaderStage::Compute; break;
    default:
      RDCERR("Unexpected DXBC shader type %u", dxbc->m_Type);
      refl->stage = ShaderStage::Vertex;
      break;
  }

  refl->entryPoint = "main";

  if(dxbc->GetDebugInfo())
  {
    refl->entryPoint = dxbc->GetDebugInfo()->GetEntryFunction();

    refl->debugInfo.encoding = ShaderEncoding::HLSL;

    refl->debugInfo.compileFlags = dxbc->GetDebugInfo()->GetShaderCompileFlags();

    refl->debugInfo.files.resize(dxbc->GetDebugInfo()->Files.size());
    for(size_t i = 0; i < dxbc->GetDebugInfo()->Files.size(); i++)
    {
      refl->debugInfo.files[i].filename = dxbc->GetDebugInfo()->Files[i].first;
      refl->debugInfo.files[i].contents = dxbc->GetDebugInfo()->Files[i].second;
    }

    rdcstr entry = dxbc->GetDebugInfo()->GetEntryFunction();
    if(entry.empty())
      entry = "main";

    // assume the debug info put the file with the entry point at the start. SDBG seems to do this
    // by default, and SPDB has an extra sorting step that probably maybe possibly does this.
  }
  else
  {
    // ensure we at least have shader compiler flags to indicate the right profile
    rdcstr profile;
    switch(dxbc->m_Type)
    {
      case DXBC::ShaderType::Pixel: profile = "ps"; break;
      case DXBC::ShaderType::Vertex: profile = "vs"; break;
      case DXBC::ShaderType::Geometry: profile = "gs"; break;
      case DXBC::ShaderType::Hull: profile = "hs"; break;
      case DXBC::ShaderType::Domain: profile = "ds"; break;
      case DXBC::ShaderType::Compute: profile = "cs"; break;
      default: profile = "xx"; break;
    }
    profile += StringFormat::Fmt("_%u_%u", dxbc->m_Version.Major, dxbc->m_Version.Minor);

    refl->debugInfo.compileFlags = DXBC::EncodeFlags(0, profile);
  }

  if(dxbc->GetDXBCByteCode())
  {
    refl->debugInfo.debuggable = true;
  }
  else
  {
    refl->debugInfo.debuggable = false;

    if(dxbc->GetDXILByteCode())
      refl->debugInfo.debugStatus = "Debugging DXIL is not supported";
    else
      refl->debugInfo.debugStatus = "Shader contains no recognised bytecode";
  }

  refl->encoding = ShaderEncoding::DXBC;
  refl->rawBytes = dxbc->m_ShaderBlob;

  refl->dispatchThreadsDimension[0] = dxbc->GetReflection()->DispatchThreadsDimension[0];
  refl->dispatchThreadsDimension[1] = dxbc->GetReflection()->DispatchThreadsDimension[1];
  refl->dispatchThreadsDimension[2] = dxbc->GetReflection()->DispatchThreadsDimension[2];

  refl->inputSignature = dxbc->GetReflection()->InputSig;
  refl->outputSignature = dxbc->GetReflection()->OutputSig;

  mapping->inputAttributes.resize(D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
  for(int s = 0; s < D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; s++)
    mapping->inputAttributes[s] = s;

  mapping->constantBlocks.resize(dxbc->GetReflection()->CBuffers.size());
  refl->constantBlocks.resize(dxbc->GetReflection()->CBuffers.size());
  for(size_t i = 0; i < dxbc->GetReflection()->CBuffers.size(); i++)
  {
    ConstantBlock &cb = refl->constantBlocks[i];

    cb.name = dxbc->GetReflection()->CBuffers[i].name;
    cb.bufferBacked = true;
    cb.byteSize = dxbc->GetReflection()->CBuffers[i].descriptor.byteSize;
    cb.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = dxbc->GetReflection()->CBuffers[i].bindCount;
    map.bindset = dxbc->GetReflection()->CBuffers[i].space;
    map.bind = dxbc->GetReflection()->CBuffers[i].reg;
    map.used = true;

    mapping->constantBlocks[i] = map;

    cb.variables.reserve(dxbc->GetReflection()->CBuffers[i].variables.size());
    for(size_t v = 0; v < dxbc->GetReflection()->CBuffers[i].variables.size(); v++)
    {
      cb.variables.push_back(
          MakeConstantBufferVariable(dxbc->GetReflection()->CBuffers[i].variables[v]));
    }
  }

  mapping->samplers.resize(dxbc->GetReflection()->Samplers.size());
  refl->samplers.resize(dxbc->GetReflection()->Samplers.size());
  for(size_t i = 0; i < dxbc->GetReflection()->Samplers.size(); i++)
  {
    ShaderSampler &s = refl->samplers[i];

    s.name = dxbc->GetReflection()->Samplers[i].name;
    s.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = 1;
    map.bindset = dxbc->GetReflection()->Samplers[i].space;
    map.bind = dxbc->GetReflection()->Samplers[i].reg;
    map.used = true;

    mapping->samplers[i] = map;
  }

  mapping->readOnlyResources.resize(dxbc->GetReflection()->SRVs.size());
  refl->readOnlyResources.resize(dxbc->GetReflection()->SRVs.size());
  MakeResourceList(true, dxbc, dxbc->GetReflection()->SRVs, mapping->readOnlyResources,
                   refl->readOnlyResources);

  mapping->readWriteResources.resize(dxbc->GetReflection()->UAVs.size());
  refl->readWriteResources.resize(dxbc->GetReflection()->UAVs.size());
  MakeResourceList(false, dxbc, dxbc->GetReflection()->UAVs, mapping->readWriteResources,
                   refl->readWriteResources);

  uint32_t numInterfaces = 0;
  for(size_t i = 0; i < dxbc->GetReflection()->Interfaces.variables.size(); i++)
    numInterfaces = RDCMAX(dxbc->GetReflection()->Interfaces.variables[i].offset + 1, numInterfaces);

  refl->interfaces.resize(numInterfaces);
  for(size_t i = 0; i < dxbc->GetReflection()->Interfaces.variables.size(); i++)
    refl->interfaces[dxbc->GetReflection()->Interfaces.variables[i].offset] =
        dxbc->GetReflection()->Interfaces.variables[i].name;
}
