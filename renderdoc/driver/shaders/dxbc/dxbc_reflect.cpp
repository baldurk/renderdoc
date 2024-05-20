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
#include "driver/shaders/dxil/dxil_bytecode.h"
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
                             rdcarray<ShaderResource> &refl)
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
                     r.dimension != DXBC::ShaderInputBind::DIM_BUFFEREX &&
                     r.dimension != DXBC::ShaderInputBind::DIM_RTAS);
    res.isReadOnly = srv;

    switch(r.dimension)
    {
      default:
      case DXBC::ShaderInputBind::DIM_UNKNOWN: res.textureType = TextureType::Unknown; break;
      case DXBC::ShaderInputBind::DIM_BUFFER:
      case DXBC::ShaderInputBind::DIM_BUFFEREX:
      case DXBC::ShaderInputBind::DIM_RTAS: res.textureType = TextureType::Buffer; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE1D: res.textureType = TextureType::Texture1D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY:
        res.textureType = TextureType::Texture1DArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2D: res.textureType = TextureType::Texture2D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY:
        res.textureType = TextureType::Texture2DArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DMS:
        res.textureType = TextureType::Texture2DMS;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY:
        res.textureType = TextureType::Texture2DMSArray;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURE3D: res.textureType = TextureType::Texture3D; break;
      case DXBC::ShaderInputBind::DIM_TEXTURECUBE:
        res.textureType = TextureType::TextureCube;
        break;
      case DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY:
        res.textureType = TextureType::TextureCubeArray;
        break;
    }

    if(r.type == DXBC::ShaderInputBind::TYPE_RTAS)
    {
      res.variableType.rows = res.variableType.columns = 1;
      res.variableType.elements = 1;
      res.variableType.baseType = VarType::Unknown;
      res.variableType.name = "RaytracingAccelerationStructure";
    }
    else if(r.type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS ||
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

    res.fixedBindNumber = r.reg;
    res.fixedBindSetOrSpace = r.space;
    res.bindArraySize = r.bindCount == 0 ? ~0U : r.bindCount;

    if(r.type == DXBC::ShaderInputBind::TYPE_RTAS)
    {
      res.descriptorType = DescriptorType::AccelerationStructure;
    }
    else if(res.isReadOnly)
    {
      res.descriptorType = DescriptorType::Image;
      if(!res.isTexture)
        res.descriptorType = (r.type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
                              r.type == DXBC::ShaderInputBind::TYPE_TEXTURE)
                                 ? DescriptorType::TypedBuffer
                                 : DescriptorType::Buffer;
    }
    else
    {
      res.descriptorType = DescriptorType::ReadWriteImage;
      if(!res.isTexture)
        res.descriptorType = r.type == DXBC::ShaderInputBind::TYPE_UAV_RWTYPED
                                 ? DescriptorType::ReadWriteTypedBuffer
                                 : DescriptorType::ReadWriteBuffer;
    }

    refl[i] = res;
  }
}

void MakeShaderReflection(DXBC::DXBCContainer *dxbc, const ShaderEntryPoint &entry,
                          ShaderReflection *refl)
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
    case DXBC::ShaderType::Library: refl->stage = entry.stage; break;
    default:
      RDCERR("Unexpected DXBC shader type %u", dxbc->m_Type);
      refl->stage = ShaderStage::Vertex;
      break;
  }

  refl->debugInfo.entrySourceName = refl->entryPoint = "main";

  if(dxbc->GetDebugInfo())
  {
    refl->debugInfo.encoding = ShaderEncoding::HLSL;

    refl->debugInfo.sourceDebugInformation = true;

    refl->debugInfo.compileFlags = dxbc->GetDebugInfo()->GetShaderCompileFlags();

    refl->debugInfo.files = dxbc->GetDebugInfo()->Files;

    dxbc->GetDebugInfo()->GetLineInfo(~0U, ~0U, refl->debugInfo.entryLocation);

    rdcstr entryFunc = entry.name;
    if(entryFunc.empty())
      entryFunc = dxbc->GetDebugInfo()->GetEntryFunction();
    if(entryFunc.empty())
      entryFunc = "main";

    refl->debugInfo.entrySourceName = refl->entryPoint = entryFunc;

    // demangle DXIL source names for display
    refl->debugInfo.entrySourceName = DXBC::BasicDemangle(refl->entryPoint);

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
      case DXBC::ShaderType::Library: profile = "lib"; break;
      default: profile = "xx"; break;
    }
    profile += StringFormat::Fmt("_%u_%u", dxbc->m_Version.Major, dxbc->m_Version.Minor);

    refl->debugInfo.compileFlags = DXBC::EncodeFlags(0, profile);
  }

  if(dxbc->GetDXBCByteCode())
  {
    refl->debugInfo.debugStatus = dxbc->GetDXBCByteCode()->GetDebugStatus();
  }
  else
  {
    if(dxbc->GetDXILByteCode())
      refl->debugInfo.debugStatus = dxbc->GetDXILByteCode()->GetDebugStatus();
    else
      refl->debugInfo.debugStatus = "Shader contains no recognised bytecode";
  }
  refl->debugInfo.debuggable = refl->debugInfo.debugStatus.empty();

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

  refl->constantBlocks.resize(dxbcRefl->CBuffers.size());
  for(size_t i = 0; i < dxbcRefl->CBuffers.size(); i++)
  {
    ConstantBlock &cb = refl->constantBlocks[i];

    cb.name = dxbcRefl->CBuffers[i].name;
    cb.bufferBacked = true;
    cb.byteSize = dxbcRefl->CBuffers[i].descriptor.byteSize;

    cb.fixedBindNumber = dxbcRefl->CBuffers[i].reg;
    cb.fixedBindSetOrSpace = dxbcRefl->CBuffers[i].space;
    cb.bindArraySize = dxbcRefl->CBuffers[i].bindCount;

    cb.variables.reserve(dxbcRefl->CBuffers[i].variables.size());
    for(size_t v = 0; v < dxbcRefl->CBuffers[i].variables.size(); v++)
    {
      cb.variables.push_back(MakeConstantBufferVariable(true, dxbcRefl->CBuffers[i].variables[v]));
    }

    FixupEmptyStructs(cb.variables);
  }

  refl->samplers.resize(dxbcRefl->Samplers.size());
  for(size_t i = 0; i < dxbcRefl->Samplers.size(); i++)
  {
    ShaderSampler &s = refl->samplers[i];

    s.name = dxbcRefl->Samplers[i].name;

    s.fixedBindNumber = dxbcRefl->Samplers[i].reg;
    s.fixedBindSetOrSpace = dxbcRefl->Samplers[i].space;
    s.bindArraySize = dxbcRefl->Samplers[i].bindCount;
  }

  refl->readOnlyResources.resize(dxbcRefl->SRVs.size());
  MakeResourceList(true, dxbc, dxbcRefl->SRVs, refl->readOnlyResources);

  refl->readWriteResources.resize(dxbcRefl->UAVs.size());
  MakeResourceList(false, dxbc, dxbcRefl->UAVs, refl->readWriteResources);

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

  DXBC::CBufferVariableType RayPayload = dxbc->GetRayPayload(entry);
  DXBC::CBufferVariableType RayAttributes = dxbc->GetRayAttributes(entry);

  refl->rayPayload.bufferBacked = false;
  refl->rayPayload.name = RayPayload.name;
  refl->rayPayload.variables.reserve(RayPayload.members.size());
  for(size_t v = 0; v < RayPayload.members.size(); v++)
  {
    refl->rayPayload.variables.push_back(MakeConstantBufferVariable(false, RayPayload.members[v]));
  }

  refl->rayAttributes.bufferBacked = false;
  refl->rayAttributes.name = RayAttributes.name;
  refl->rayAttributes.variables.reserve(RayAttributes.members.size());
  for(size_t v = 0; v < RayAttributes.members.size(); v++)
  {
    refl->rayAttributes.variables.push_back(
        MakeConstantBufferVariable(false, RayAttributes.members[v]));
  }
}
