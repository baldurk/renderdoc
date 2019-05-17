/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "dxbc_inspect.h"

static ShaderConstant MakeConstantBufferVariable(const DXBC::CBufferVariable &var);

static ShaderVariableType MakeShaderVariableType(DXBC::CBufferVariableType type)
{
  ShaderVariableType ret;

  switch(type.descriptor.type)
  {
    // D3D treats all cbuffer variables as 32-bit regardless of declaration
    case DXBC::VARTYPE_MIN12INT:
    case DXBC::VARTYPE_MIN16INT:
    case DXBC::VARTYPE_INT: ret.descriptor.type = VarType::SInt; break;
    case DXBC::VARTYPE_BOOL:
    case DXBC::VARTYPE_MIN16UINT:
    case DXBC::VARTYPE_UINT: ret.descriptor.type = VarType::UInt; break;
    case DXBC::VARTYPE_DOUBLE: ret.descriptor.type = VarType::Double; break;
    case DXBC::VARTYPE_FLOAT:
    case DXBC::VARTYPE_MIN8FLOAT:
    case DXBC::VARTYPE_MIN10FLOAT:
    case DXBC::VARTYPE_MIN16FLOAT:
    default: ret.descriptor.type = VarType::Float; break;
  }
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
    ret.descriptor.arrayByteStride = type.descriptor.bytesize / RDCMAX(1U, type.descriptor.elements);
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
  ret.byteOffset = var.descriptor.offset;
  ret.defaultValue = 0;
  ret.type = MakeShaderVariableType(var.type);

  return ret;
}

static void MakeResourceList(bool srv, DXBC::DXBCFile *dxbc,
                             const std::vector<DXBC::ShaderInputBind> &in,
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

    if(r.retType != DXBC::RETURN_TYPE_UNKNOWN && r.retType != DXBC::RETURN_TYPE_MIXED &&
       r.retType != DXBC::RETURN_TYPE_CONTINUED)
    {
      res.variableType.descriptor.rows = 1;
      res.variableType.descriptor.columns = (uint8_t)r.numSamples;
      res.variableType.descriptor.elements = 1;

      std::string name;

      switch(r.retType)
      {
        case DXBC::RETURN_TYPE_UNORM: name = "unorm float"; break;
        case DXBC::RETURN_TYPE_SNORM: name = "snorm float"; break;
        case DXBC::RETURN_TYPE_SINT: name = "int"; break;
        case DXBC::RETURN_TYPE_UINT: name = "uint"; break;
        case DXBC::RETURN_TYPE_FLOAT: name = "float"; break;
        case DXBC::RETURN_TYPE_DOUBLE: name = "double"; break;
        default: name = "unknown"; break;
      }

      if(r.numSamples > 1)
        name += StringFormat::Fmt("%u", r.numSamples);

      res.variableType.descriptor.name = name;
    }
    else
    {
      if(dxbc->m_ResourceBinds.find(r.name) != dxbc->m_ResourceBinds.end())
      {
        res.variableType = MakeShaderVariableType(dxbc->m_ResourceBinds[r.name]);
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

void MakeShaderReflection(DXBC::DXBCFile *dxbc, ShaderReflection *refl,
                          ShaderBindpointMapping *mapping)
{
  if(dxbc == NULL || !RenderDoc::Inst().IsReplayApp())
    return;

  switch(dxbc->m_Type)
  {
    case D3D11_ShaderType_Pixel: refl->stage = ShaderStage::Pixel; break;
    case D3D11_ShaderType_Vertex: refl->stage = ShaderStage::Vertex; break;
    case D3D11_ShaderType_Geometry: refl->stage = ShaderStage::Geometry; break;
    case D3D11_ShaderType_Hull: refl->stage = ShaderStage::Hull; break;
    case D3D11_ShaderType_Domain: refl->stage = ShaderStage::Domain; break;
    case D3D11_ShaderType_Compute: refl->stage = ShaderStage::Compute; break;
    default:
      RDCERR("Unexpected DXBC shader type %u", dxbc->m_Type);
      refl->stage = ShaderStage::Vertex;
      break;
  }

  refl->entryPoint = "main";

  if(dxbc->m_DebugInfo)
  {
    refl->entryPoint = dxbc->m_DebugInfo->GetEntryFunction();

    refl->debugInfo.encoding = ShaderEncoding::HLSL;

    refl->debugInfo.compileFlags = DXBC::EncodeFlags(dxbc->m_DebugInfo);

    refl->debugInfo.files.resize(dxbc->m_DebugInfo->Files.size());
    for(size_t i = 0; i < dxbc->m_DebugInfo->Files.size(); i++)
    {
      refl->debugInfo.files[i].filename = dxbc->m_DebugInfo->Files[i].first;
      refl->debugInfo.files[i].contents = dxbc->m_DebugInfo->Files[i].second;
    }

    std::string entry = dxbc->m_DebugInfo->GetEntryFunction();
    if(entry.empty())
      entry = "main";

    // assume the debug info put the file with the entry point at the start. SDBG seems to do this
    // by default, and SPDB has an extra sorting step that probably maybe possibly does this.
  }

  refl->encoding = ShaderEncoding::DXBC;
  refl->rawBytes = dxbc->m_ShaderBlob;

  refl->dispatchThreadsDimension[0] = dxbc->DispatchThreadsDimension[0];
  refl->dispatchThreadsDimension[1] = dxbc->DispatchThreadsDimension[1];
  refl->dispatchThreadsDimension[2] = dxbc->DispatchThreadsDimension[2];

  refl->inputSignature = dxbc->m_InputSig;
  refl->outputSignature = dxbc->m_OutputSig;

  mapping->inputAttributes.resize(D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
  for(int s = 0; s < D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; s++)
    mapping->inputAttributes[s] = s;

  mapping->constantBlocks.resize(dxbc->m_CBuffers.size());
  refl->constantBlocks.resize(dxbc->m_CBuffers.size());
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    ConstantBlock &cb = refl->constantBlocks[i];

    cb.name = dxbc->m_CBuffers[i].name;
    cb.bufferBacked = true;
    cb.byteSize = dxbc->m_CBuffers[i].descriptor.byteSize;
    cb.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = 1;
    map.bindset = dxbc->m_CBuffers[i].space;
    map.bind = dxbc->m_CBuffers[i].reg;
    map.used = true;

    mapping->constantBlocks[i] = map;

    cb.variables.reserve(dxbc->m_CBuffers[i].variables.size());
    for(size_t v = 0; v < dxbc->m_CBuffers[i].variables.size(); v++)
    {
      cb.variables.push_back(MakeConstantBufferVariable(dxbc->m_CBuffers[i].variables[v]));
    }
  }

  mapping->samplers.resize(dxbc->m_Samplers.size());
  refl->samplers.resize(dxbc->m_Samplers.size());
  for(size_t i = 0; i < dxbc->m_Samplers.size(); i++)
  {
    ShaderSampler &s = refl->samplers[i];

    s.name = dxbc->m_Samplers[i].name;
    s.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = 1;
    map.bindset = dxbc->m_Samplers[i].space;
    map.bind = dxbc->m_Samplers[i].reg;
    map.used = true;

    mapping->samplers[i] = map;
  }

  mapping->readOnlyResources.resize(dxbc->m_SRVs.size());
  refl->readOnlyResources.resize(dxbc->m_SRVs.size());
  MakeResourceList(true, dxbc, dxbc->m_SRVs, mapping->readOnlyResources, refl->readOnlyResources);

  mapping->readWriteResources.resize(dxbc->m_UAVs.size());
  refl->readWriteResources.resize(dxbc->m_UAVs.size());
  MakeResourceList(false, dxbc, dxbc->m_UAVs, mapping->readWriteResources, refl->readWriteResources);

  uint32_t numInterfaces = 0;
  for(size_t i = 0; i < dxbc->m_Interfaces.variables.size(); i++)
    numInterfaces = RDCMAX(dxbc->m_Interfaces.variables[i].descriptor.offset + 1, numInterfaces);

  refl->interfaces.resize(numInterfaces);
  for(size_t i = 0; i < dxbc->m_Interfaces.variables.size(); i++)
    refl->interfaces[dxbc->m_Interfaces.variables[i].descriptor.offset] =
        dxbc->m_Interfaces.variables[i].name;
}
