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

#include "dxbc_reflect.h"
#include "common/formatting.h"
#include "core/core.h"
#include "dxbc_bytecode.h"
#include "dxbc_container.h"

static ShaderConstant MakeConstantBufferVariable(bool cbufferPacking,
                                                 const DXBC::CBufferVariable &var);

static void FixupEmptyStructs(rdcarray<ShaderConstant> &members)
{
  for(size_t i = 0; i < members.size(); i++)
  {
    if(members[i].byteOffset == ~0U)
    {
      // don't try to calculate offset for trailing empty structs, just delete them
      if(i == members.size() - 1)
        members.pop_back();
      // other empty struct members take the offset of the next member
      else
        members[i].byteOffset = members[i + 1].byteOffset;
    }
  }
}

static ShaderConstantType MakeShaderConstantType(bool cbufferPacking, DXBC::CBufferVariableType type)
{
  ShaderConstantType ret;

  ret.baseType = type.varType;
  ret.rows = (uint8_t)type.rows;
  ret.columns = (uint8_t)type.cols;
  ret.elements = type.elements;
  ret.name = type.name;
  if(type.varClass == DXBC::CLASS_MATRIX_ROWS || type.varClass == DXBC::CLASS_VECTOR ||
     type.varClass == DXBC::CLASS_SCALAR)
    ret.flags |= ShaderVariableFlags::RowMajorMatrix;

  uint32_t baseElemSize = (ret.baseType == VarType::Double) ? 8 : 4;

  // in D3D matrices in cbuffers always take up a float4 per row/column. Structured buffers in
  // SRVs/UAVs are tightly packed
  if(cbufferPacking)
    ret.matrixByteStride = uint8_t(baseElemSize * 4);
  else
    ret.matrixByteStride = uint8_t(baseElemSize * (ret.RowMajor() ? ret.columns : ret.rows));

  if(type.varClass == DXBC::CLASS_STRUCT)
  {
    // fxc's reported byte size for UAV structs is not reliable (booo). It seems to assume some
    // padding that doesn't exist, especially in arrays. So e.g.:
    // struct nested_with_padding
    // {
    //   float a; float4 b; float c; float3 d[4];
    // };
    // is tightly packed and only contains 1+4+1+4*3 floats = 18*4 = 72 bytes
    // however an array of nested_with_padding foo[2] will have size listed as 176 bytes.
    //
    // fortunately, since packing is tight we can look at the 'columns' field which is the number of
    // floats in the struct, and multiply that
    uint32_t stride = type.bytesize / RDCMAX(1U, type.elements);
    if(!cbufferPacking)
    {
      stride = type.cols * sizeof(float);
      // the exception is empty structs have 1 cols, probably because of a max(1,...) somewhere
      if(type.bytesize == 0)
        stride = 0;
    }

    ret.arrayByteStride = stride;
    // in D3D only cbuffers have 16-byte aligned structs
    if(cbufferPacking)
      ret.arrayByteStride = AlignUp16(ret.arrayByteStride);

    ret.rows = ret.columns = 0;

    ret.baseType = VarType::Struct;
  }
  else
  {
    if(ret.RowMajor())
      ret.arrayByteStride = ret.matrixByteStride * ret.rows;
    else
      ret.arrayByteStride = ret.matrixByteStride * ret.columns;
  }

  ret.members.reserve(type.members.size());
  for(size_t i = 0; i < type.members.size(); i++)
    ret.members.push_back(MakeConstantBufferVariable(cbufferPacking, type.members[i]));

  FixupEmptyStructs(ret.members);

  if(!ret.members.empty())
  {
    ret.rows = 0;
    ret.columns = 0;
  }

  return ret;
}

static ShaderConstant MakeConstantBufferVariable(bool cbufferPacking, const DXBC::CBufferVariable &var)
{
  ShaderConstant ret;

  ret.name = var.name;
  ret.byteOffset = var.offset;
  ret.defaultValue = 0;
  ret.type = MakeShaderConstantType(cbufferPacking, var.type);

  // fxc emits negative values for offsets of empty structs sometimes. Replace that with a single
  // value so we can say 'use the previous value'
  if(ret.type.baseType == VarType::Struct && ret.type.members.empty() && ret.byteOffset > 0xF0000000)
    ret.byteOffset = ~0U;

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
      res.variableType.rows = res.variableType.columns = 1;
      res.variableType.elements = 1;
      res.variableType.baseType = VarType::UByte;
      res.variableType.name = "byte";
    }
    else if(r.retType != DXBC::RETURN_TYPE_UNKNOWN && r.retType != DXBC::RETURN_TYPE_MIXED &&
            r.retType != DXBC::RETURN_TYPE_CONTINUED)
    {
      res.variableType.rows = 1;
      res.variableType.columns = (uint8_t)r.numComps;
      res.variableType.elements = 1;

      rdcstr name;

      switch(r.retType)
      {
        case DXBC::RETURN_TYPE_UNORM:
          name = "unorm float";
          res.variableType.baseType = VarType::Float;
          break;
        case DXBC::RETURN_TYPE_SNORM:
          name = "snorm float";
          res.variableType.baseType = VarType::Float;
          break;
        case DXBC::RETURN_TYPE_SINT:
          name = "int";
          res.variableType.baseType = VarType::SInt;
          break;
        case DXBC::RETURN_TYPE_UINT:
          name = "uint";
          res.variableType.baseType = VarType::UInt;
          break;
        case DXBC::RETURN_TYPE_FLOAT:
          name = "float";
          res.variableType.baseType = VarType::Float;
          break;
        case DXBC::RETURN_TYPE_DOUBLE:
          name = "double";
          res.variableType.baseType = VarType::Double;
          break;
        default: name = "unknown"; break;
      }

      if(r.numComps > 1)
        name += StringFormat::Fmt("%u", r.numComps);

      res.variableType.name = name;
    }
    else
    {
      auto it = dxbc->GetReflection()->ResourceBinds.find(r.name);
      if(it != dxbc->GetReflection()->ResourceBinds.end())
      {
        res.variableType = MakeShaderConstantType(false, it->second);
      }
      else
      {
        res.variableType.rows = 0;
        res.variableType.columns = 0;
        res.variableType.elements = 0;
        res.variableType.name = "";
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
    case DXBC::ShaderType::Amplification: refl->stage = ShaderStage::Amplification; break;
    case DXBC::ShaderType::Mesh: refl->stage = ShaderStage::Mesh; break;
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

    refl->debugInfo.sourceDebugInformation = true;

    refl->debugInfo.compileFlags = dxbc->GetDebugInfo()->GetShaderCompileFlags();

    refl->debugInfo.files = dxbc->GetDebugInfo()->Files;

    dxbc->GetDebugInfo()->GetLineInfo(~0U, ~0U, refl->debugInfo.entryLocation);

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
    refl->debugInfo.debugStatus = dxbc->GetDXBCByteCode()->GetDebugStatus();

    refl->debugInfo.debuggable = refl->debugInfo.debugStatus.empty();
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
  refl->debugInfo.compiler = KnownShaderTool::fxc;
  if(dxbc->GetDXILByteCode())
  {
    refl->encoding = ShaderEncoding::DXIL;
    refl->debugInfo.compiler = KnownShaderTool::dxcDXIL;
  }
  refl->rawBytes = dxbc->GetShaderBlob();

  const DXBC::Reflection *dxbcRefl = dxbc->GetReflection();

  refl->dispatchThreadsDimension[0] = dxbcRefl->DispatchThreadsDimension[0];
  refl->dispatchThreadsDimension[1] = dxbcRefl->DispatchThreadsDimension[1];
  refl->dispatchThreadsDimension[2] = dxbcRefl->DispatchThreadsDimension[2];

  refl->inputSignature = dxbcRefl->InputSig;
  refl->outputSignature = dxbcRefl->OutputSig;

  switch(dxbc->GetOutputTopology())
  {
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: refl->outputTopology = Topology::PointList; break;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST: refl->outputTopology = Topology::LineList; break;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: refl->outputTopology = Topology::LineStrip; break;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST: refl->outputTopology = Topology::TriangleList; break;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
      refl->outputTopology = Topology::TriangleStrip;
      break;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: refl->outputTopology = Topology::LineList_Adj; break;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
      refl->outputTopology = Topology::LineStrip_Adj;
      break;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
      refl->outputTopology = Topology::TriangleList_Adj;
      break;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
      refl->outputTopology = Topology::TriangleStrip_Adj;
      break;
    default: refl->outputTopology = Topology::Unknown; break;
  }

  mapping->inputAttributes.resize(D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
  for(int s = 0; s < D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; s++)
    mapping->inputAttributes[s] = s;

  mapping->constantBlocks.resize(dxbcRefl->CBuffers.size());
  refl->constantBlocks.resize(dxbcRefl->CBuffers.size());
  for(size_t i = 0; i < dxbcRefl->CBuffers.size(); i++)
  {
    ConstantBlock &cb = refl->constantBlocks[i];

    cb.name = dxbcRefl->CBuffers[i].name;
    cb.bufferBacked = true;
    cb.byteSize = dxbcRefl->CBuffers[i].descriptor.byteSize;
    cb.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = dxbcRefl->CBuffers[i].bindCount;
    map.bindset = dxbcRefl->CBuffers[i].space;
    map.bind = dxbcRefl->CBuffers[i].reg;
    map.used = true;

    mapping->constantBlocks[i] = map;

    cb.variables.reserve(dxbcRefl->CBuffers[i].variables.size());
    for(size_t v = 0; v < dxbcRefl->CBuffers[i].variables.size(); v++)
    {
      cb.variables.push_back(MakeConstantBufferVariable(true, dxbcRefl->CBuffers[i].variables[v]));
    }

    FixupEmptyStructs(cb.variables);
  }

  mapping->samplers.resize(dxbcRefl->Samplers.size());
  refl->samplers.resize(dxbcRefl->Samplers.size());
  for(size_t i = 0; i < dxbcRefl->Samplers.size(); i++)
  {
    ShaderSampler &s = refl->samplers[i];

    s.name = dxbcRefl->Samplers[i].name;
    s.bindPoint = (int32_t)i;

    Bindpoint map;
    map.arraySize = 1;
    map.bindset = dxbcRefl->Samplers[i].space;
    map.bind = dxbcRefl->Samplers[i].reg;
    map.used = true;

    mapping->samplers[i] = map;
  }

  mapping->readOnlyResources.resize(dxbcRefl->SRVs.size());
  refl->readOnlyResources.resize(dxbcRefl->SRVs.size());
  MakeResourceList(true, dxbc, dxbcRefl->SRVs, mapping->readOnlyResources, refl->readOnlyResources);

  mapping->readWriteResources.resize(dxbcRefl->UAVs.size());
  refl->readWriteResources.resize(dxbcRefl->UAVs.size());
  MakeResourceList(false, dxbc, dxbcRefl->UAVs, mapping->readWriteResources,
                   refl->readWriteResources);

  uint32_t numInterfaces = 0;
  for(size_t i = 0; i < dxbcRefl->Interfaces.variables.size(); i++)
    numInterfaces = RDCMAX(dxbcRefl->Interfaces.variables[i].offset + 1, numInterfaces);

  refl->interfaces.resize(numInterfaces);
  for(size_t i = 0; i < dxbcRefl->Interfaces.variables.size(); i++)
    refl->interfaces[dxbcRefl->Interfaces.variables[i].offset] =
        dxbcRefl->Interfaces.variables[i].name;

  refl->taskPayload.bufferBacked = false;
  refl->taskPayload.name = dxbcRefl->TaskPayload.name;
  refl->taskPayload.variables.reserve(dxbcRefl->TaskPayload.members.size());
  for(size_t v = 0; v < dxbcRefl->TaskPayload.members.size(); v++)
  {
    refl->taskPayload.variables.push_back(
        MakeConstantBufferVariable(false, dxbcRefl->TaskPayload.members[v]));
  }
}
