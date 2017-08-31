/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include "replay/type_helpers.h"
#include "dxbc_inspect.h"

static ShaderConstant MakeConstantBufferVariable(const DXBC::CBufferVariable &var, uint32_t &offset);

static ShaderVariableType MakeShaderVariableType(DXBC::CBufferVariableType type, uint32_t &offset)
{
  ShaderVariableType ret;

  switch(type.descriptor.type)
  {
    case DXBC::VARTYPE_MIN12INT:
    case DXBC::VARTYPE_MIN16INT:
    case DXBC::VARTYPE_INT: ret.descriptor.type = VarType::Int; break;
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
  ret.descriptor.cols = (uint8_t)type.descriptor.cols;
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
    refl->DebugInfo.compileFlags = DXBC::EncodeFlags(dxbc->m_DebugInfo);

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

  create_array_uninit(mapping->InputAttributes, D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
  for(int s = 0; s < D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; s++)
    mapping->InputAttributes[s] = s;

  create_array_uninit(mapping->ConstantBlocks, dxbc->m_CBuffers.size());
  create_array_uninit(refl->ConstantBlocks, dxbc->m_CBuffers.size());
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    ConstantBlock &cb = refl->ConstantBlocks[i];

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
      res.variableType.descriptor.cols = (uint8_t)r.numSamples;
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

      name += StringFormat::Fmt("%u", r.numSamples);

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
