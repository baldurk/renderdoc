/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "dxbc_bytecode.h"
#include "dxbc_container.h"

namespace DXBCBytecode
{
Program::Program(const byte *bytes, size_t length)
{
  RDCASSERT((length % 4) == 0);
  m_HexDump.resize(length / 4);
  memcpy(m_HexDump.data(), bytes, length);

  FetchTypeVersion();
}

DXBC::Reflection *Program::GuessReflection()
{
  DisassembleHexDump();

  // we don't store this, since it's just a guess. If our m_Reflection is NULL that indicates no
  // useful reflection is present
  DXBC::Reflection *ret = new DXBC::Reflection;

  char buf[64] = {0};

  for(size_t i = 0; i < m_Declarations.size(); i++)
  {
    Declaration &dcl = m_Declarations[i];

    switch(dcl.declaration)
    {
      case OPCODE_DCL_SAMPLER:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_SAMPLER);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        StringFormat::snprintf(buf, 63, "sampler%u", idx);

        desc.name = buf;
        desc.type = DXBC::ShaderInputBind::TYPE_SAMPLER;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = dcl.samplerMode == SAMPLER_MODE_COMPARISON ? 2 : 0;
        desc.retType = DXBC::RETURN_TYPE_UNKNOWN;
        desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN;
        desc.numSamples = 0;

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        ret->Samplers.push_back(desc);

        break;
      }
      case OPCODE_DCL_RESOURCE:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_RESOURCE);
        RDCASSERT(dcl.operand.indices.size() == 1);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        StringFormat::snprintf(buf, 63, "texture%u", idx);

        desc.name = buf;
        desc.type = DXBC::ShaderInputBind::TYPE_TEXTURE;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = 0;
        desc.retType = dcl.resType[0];

        switch(dcl.dim)
        {
          case RESOURCE_DIMENSION_BUFFER: desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER; break;
          case RESOURCE_DIMENSION_TEXTURE1D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1D;
            break;
          case RESOURCE_DIMENSION_TEXTURE2D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2D;
            break;
          case RESOURCE_DIMENSION_TEXTURE3D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE3D;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBE:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBE;
            break;
          case RESOURCE_DIMENSION_TEXTURE1DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMS:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMS;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY;
            break;
          default: desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN; break;
        }
        desc.numSamples = dcl.sampleCount;

        // can't tell, fxc seems to default to 4
        if(desc.dimension == DXBC::ShaderInputBind::DIM_BUFFER)
          desc.numSamples = 4;

        RDCASSERT(desc.dimension != DXBC::ShaderInputBind::DIM_UNKNOWN);

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        ret->SRVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
      case OPCODE_DCL_RESOURCE_RAW:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_RESOURCE ||
                  dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
        RDCASSERT(dcl.operand.indices.size() == 1);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        StringFormat::snprintf(buf, 63, "%sbytebuffer%u",
                               dcl.operand.type != TYPE_RESOURCE ? "rw" : "", idx);

        desc.name = buf;
        desc.type = dcl.operand.type == TYPE_RESOURCE
                        ? DXBC::ShaderInputBind::TYPE_BYTEADDRESS
                        : DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = 0;
        desc.retType = DXBC::RETURN_TYPE_MIXED;
        desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER;
        desc.numSamples = 0;

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        if(dcl.operand.type == TYPE_RESOURCE)
          ret->SRVs.push_back(desc);
        else
          ret->UAVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_RESOURCE_STRUCTURED:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_RESOURCE);
        RDCASSERT(dcl.operand.indices.size() == 1);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        StringFormat::snprintf(buf, 63, "structuredbuffer%u", idx);

        desc.name = buf;
        desc.type = DXBC::ShaderInputBind::TYPE_STRUCTURED;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = 0;
        desc.retType = DXBC::RETURN_TYPE_MIXED;
        desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER;
        desc.numSamples = dcl.stride;

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        ret->SRVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
        RDCASSERT(dcl.operand.indices.size() == 1);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        StringFormat::snprintf(buf, 63, "uav%u", idx);

        desc.name = buf;
        desc.type =
            DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED;    // doesn't seem to be anything that
                                                             // determines append vs consume vs
                                                             // rwstructured
        if(dcl.hasCounter)
          desc.type = DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = 0;
        desc.retType = DXBC::RETURN_TYPE_MIXED;
        desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER;
        desc.numSamples = dcl.stride;

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        ret->UAVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
        RDCASSERT(dcl.operand.indices.size() == 1);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        StringFormat::snprintf(buf, 63, "uav%u", idx);

        desc.name = buf;
        desc.type = DXBC::ShaderInputBind::TYPE_UAV_RWTYPED;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = 0;
        desc.retType = dcl.resType[0];

        switch(dcl.dim)
        {
          case RESOURCE_DIMENSION_BUFFER: desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER; break;
          case RESOURCE_DIMENSION_TEXTURE1D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1D;
            break;
          case RESOURCE_DIMENSION_TEXTURE2D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2D;
            break;
          case RESOURCE_DIMENSION_TEXTURE3D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE3D;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBE:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBE;
            break;
          case RESOURCE_DIMENSION_TEXTURE1DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMS:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMS;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY;
            break;
          default: desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN; break;
        }
        desc.numSamples = (uint32_t)-1;

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        ret->UAVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_CONSTANT_BUFFER:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_CONSTANT_BUFFER);
        RDCASSERT(dcl.operand.indices.size() == 2);
        RDCASSERT(dcl.operand.indices[0].absolute && dcl.operand.indices[1].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;
        uint32_t numVecs = (uint32_t)dcl.operand.indices[1].index;

        StringFormat::snprintf(buf, 63, "cbuffer%u", idx);

        desc.name = buf;
        desc.type = DXBC::ShaderInputBind::TYPE_CBUFFER;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.flags = 1;
        desc.retType = DXBC::RETURN_TYPE_UNKNOWN;
        desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN;
        desc.numSamples = 0;

        if(dcl.operand.indices.size() == 3)
        {
          desc.bindCount = uint32_t(dcl.operand.indices[2].index - dcl.operand.indices[1].index);
          if(dcl.operand.indices[2].index == 0xffffffff)
            desc.bindCount = 0;
        }

        DXBC::CBuffer cb;

        cb.name = desc.name;

        cb.space = dcl.space;
        cb.reg = idx;
        cb.bindCount = desc.bindCount;

        cb.descriptor.name = cb.name;
        cb.descriptor.byteSize = numVecs * 4 * sizeof(float);
        cb.descriptor.type = DXBC::CBuffer::Descriptor::TYPE_CBUFFER;
        cb.descriptor.flags = 0;
        cb.descriptor.numVars = numVecs;

        cb.variables.reserve(numVecs);

        for(uint32_t v = 0; v < numVecs; v++)
        {
          DXBC::CBufferVariable var;

          if(desc.space > 0)
            StringFormat::snprintf(buf, 63, "cb%u_%u_v%u", desc.space, desc.reg, v);
          else
            StringFormat::snprintf(buf, 63, "cb%u_v%u", desc.reg, v);

          var.name = buf;

          var.descriptor.defaultValue.resize(4 * sizeof(float));

          var.descriptor.name = var.name;
          var.descriptor.offset = 4 * sizeof(float) * v;
          var.descriptor.flags = 0;

          var.descriptor.startTexture = (uint32_t)-1;
          var.descriptor.startSampler = (uint32_t)-1;
          var.descriptor.numSamplers = 0;
          var.descriptor.numTextures = 0;

          var.type.descriptor.bytesize = 4 * sizeof(float);
          var.type.descriptor.rows = 1;
          var.type.descriptor.cols = 4;
          var.type.descriptor.elements = 0;
          var.type.descriptor.members = 0;
          var.type.descriptor.type = DXBC::VARTYPE_FLOAT;
          var.type.descriptor.varClass = DXBC::CLASS_VECTOR;
          var.type.descriptor.name = TypeName(var.type.descriptor);

          cb.variables.push_back(var);
        }

        ret->CBuffers.push_back(cb);

        break;
      }
    }
  }

  return ret;
}

D3D_PRIMITIVE_TOPOLOGY Program::GetOutputTopology()
{
  DisassembleHexDump();

  if(m_Type != DXBC::ShaderType::Geometry && m_Type != DXBC::ShaderType::Domain)
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  for(const DXBCBytecode::Declaration &decl : m_Declarations)
  {
    if(decl.declaration == DXBCBytecode::OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
      return decl.outTopology;
    if(decl.declaration == DXBCBytecode::OPCODE_DCL_TESS_DOMAIN)
    {
      if(decl.domain == DXBCBytecode::DOMAIN_ISOLINE)
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
      else
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      break;
    }
  }

  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

};    // namespace DXBCBytecode
