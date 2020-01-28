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

#include "dxbc_bytecode.h"
#include "common/formatting.h"
#include "os/os_specific.h"
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

        desc.name = StringFormat::Fmt("sampler%u", idx);
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
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("texture%u", idx);
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
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name =
            StringFormat::Fmt("%sbytebuffer%u", dcl.operand.type != TYPE_RESOURCE ? "rw" : "", idx);
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
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("structuredbuffer%u", idx);
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
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("uav%u", idx);
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
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("uav%u", idx);
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
        RDCASSERT(dcl.operand.indices.size() == 2 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute && dcl.operand.indices[1].absolute);

        // Constant buffer declarations differ between SM5 and SM5.1.  For SM5.1, the indices are
        // logical identifier, start shader register, and end shader register. Register space and
        // buffer size are stored elsewhere in the declaration. For SM5 and earlier, the indices
        // are the shader register and buffer size (measured in float4's)
        bool isShaderModel51 = IsShaderModel51();
        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;
        uint32_t reg = isShaderModel51 ? (uint32_t)dcl.operand.indices[1].index : idx;
        uint32_t numVecs = isShaderModel51 ? dcl.float4size : (uint32_t)dcl.operand.indices[1].index;

        desc.name = StringFormat::Fmt("cbuffer%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_CBUFFER;
        desc.space = dcl.space;
        desc.reg = reg;
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

        // In addition to the register, store the identifier that we'll use to lookup during
        // debugging. For SM5.1, this is the logical identifier that correlates to the CB order in
        // the bytecode. For SM5 and earlier, it's the CB register.
        cb.identifier = idx;
        cb.space = dcl.space;
        cb.reg = reg;
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
            var.name = StringFormat::Fmt("cb%u_%u_v%u", desc.space, desc.reg, v);
          else
            var.name = StringFormat::Fmt("cb%u_v%u", desc.reg, v);

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
      default: break;
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

uint32_t Program::GetDisassemblyLine(uint32_t instruction) const
{
  return m_Instructions[RDCMIN(m_Instructions.size() - 1, (size_t)instruction)].line;
}

// see http://msdn.microsoft.com/en-us/library/windows/desktop/bb219840(v=vs.85).aspx
// for details of these opcodes
size_t NumOperands(OpcodeType op)
{
  switch(op)
  {
    case OPCODE_BREAK:
    case OPCODE_CONTINUE:
    case OPCODE_CUT:
    case OPCODE_DEFAULT:
    case OPCODE_ELSE:
    case OPCODE_EMIT:
    case OPCODE_EMITTHENCUT:
    case OPCODE_ENDIF:
    case OPCODE_ENDLOOP:
    case OPCODE_ENDSWITCH:
    case OPCODE_LOOP:
    case OPCODE_NOP:
    case OPCODE_RET:
    case OPCODE_SYNC:
    case OPCODE_ABORT:
    case OPCODE_DEBUGBREAK:

    case OPCODE_HS_CONTROL_POINT_PHASE:
    case OPCODE_HS_FORK_PHASE:
    case OPCODE_HS_JOIN_PHASE:
    case OPCODE_HS_DECLS: return 0;
    case OPCODE_BREAKC:
    case OPCODE_CONTINUEC:
    case OPCODE_CALL:
    case OPCODE_CASE:
    case OPCODE_CUT_STREAM:
    case OPCODE_DISCARD:
    case OPCODE_EMIT_STREAM:
    case OPCODE_EMITTHENCUT_STREAM:
    case OPCODE_IF:
    case OPCODE_INTERFACE_CALL:
    case OPCODE_LABEL:
    case OPCODE_RETC:
    case OPCODE_SWITCH: return 1;
    case OPCODE_BFREV:
    case OPCODE_BUFINFO:
    case OPCODE_CALLC:
    case OPCODE_COUNTBITS:
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE:
    case OPCODE_DMOV:
    case OPCODE_DTOF:
    case OPCODE_EXP:
    case OPCODE_F32TOF16:
    case OPCODE_F16TOF32:
    case OPCODE_FIRSTBIT_HI:
    case OPCODE_FIRSTBIT_LO:
    case OPCODE_FIRSTBIT_SHI:
    case OPCODE_FRC:
    case OPCODE_FTOD:
    case OPCODE_FTOI:
    case OPCODE_FTOU:
    case OPCODE_IMM_ATOMIC_ALLOC:
    case OPCODE_IMM_ATOMIC_CONSUME:
    case OPCODE_INEG:
    case OPCODE_ITOF:
    case OPCODE_LOG:
    case OPCODE_MOV:
    case OPCODE_NOT:
    case OPCODE_RCP:
    case OPCODE_ROUND_NE:
    case OPCODE_ROUND_NI:
    case OPCODE_ROUND_PI:
    case OPCODE_ROUND_Z:
    case OPCODE_RSQ:
    case OPCODE_SAMPLE_INFO:
    case OPCODE_SQRT:
    case OPCODE_UTOF:
    case OPCODE_EVAL_CENTROID:
    case OPCODE_DRCP:
    case OPCODE_DTOI:
    case OPCODE_DTOU:
    case OPCODE_ITOD:
    case OPCODE_UTOD:
    case OPCODE_CHECK_ACCESS_FULLY_MAPPED: return 2;
    case OPCODE_AND:
    case OPCODE_ADD:
    case OPCODE_ATOMIC_AND:
    case OPCODE_ATOMIC_OR:
    case OPCODE_ATOMIC_XOR:
    case OPCODE_ATOMIC_IADD:
    case OPCODE_ATOMIC_IMAX:
    case OPCODE_ATOMIC_IMIN:
    case OPCODE_ATOMIC_UMAX:
    case OPCODE_ATOMIC_UMIN:
    case OPCODE_DADD:
    case OPCODE_DIV:
    case OPCODE_DP2:
    case OPCODE_DP3:
    case OPCODE_DP4:
    case OPCODE_DEQ:
    case OPCODE_DGE:
    case OPCODE_DLT:
    case OPCODE_DMAX:
    case OPCODE_DMIN:
    case OPCODE_DMUL:
    case OPCODE_DNE:
    case OPCODE_EQ:
    case OPCODE_GE:
    case OPCODE_IADD:
    case OPCODE_IEQ:
    case OPCODE_IGE:
    case OPCODE_ILT:
    case OPCODE_IMAX:
    case OPCODE_IMIN:
    case OPCODE_INE:
    case OPCODE_ISHL:
    case OPCODE_ISHR:
    case OPCODE_LD:
    case OPCODE_LD_RAW:
    case OPCODE_LD_UAV_TYPED:
    case OPCODE_LT:
    case OPCODE_MAX:
    case OPCODE_MIN:
    case OPCODE_MUL:
    case OPCODE_NE:
    case OPCODE_OR:
    case OPCODE_RESINFO:
    case OPCODE_SAMPLE_POS:
    case OPCODE_SINCOS:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_UGE:
    case OPCODE_ULT:
    case OPCODE_UMAX:
    case OPCODE_UMIN:
    case OPCODE_USHR:
    case OPCODE_XOR:
    case OPCODE_EVAL_SNAPPED:
    case OPCODE_EVAL_SAMPLE_INDEX:
    case OPCODE_DDIV: return 3;
    case OPCODE_ATOMIC_CMP_STORE:
    case OPCODE_DMOVC:
    case OPCODE_GATHER4:
    case OPCODE_IBFE:
    case OPCODE_IMAD:
    case OPCODE_IMM_ATOMIC_IADD:
    case OPCODE_IMM_ATOMIC_AND:
    case OPCODE_IMM_ATOMIC_OR:
    case OPCODE_IMM_ATOMIC_XOR:
    case OPCODE_IMM_ATOMIC_EXCH:
    case OPCODE_IMM_ATOMIC_IMAX:
    case OPCODE_IMM_ATOMIC_IMIN:
    case OPCODE_IMM_ATOMIC_UMAX:
    case OPCODE_IMM_ATOMIC_UMIN:
    case OPCODE_IMUL:
    case OPCODE_LD_MS:
    case OPCODE_LD_STRUCTURED:
    case OPCODE_LOD:
    case OPCODE_MAD:
    case OPCODE_MOVC:
    case OPCODE_SAMPLE:
    case OPCODE_STORE_STRUCTURED:
    case OPCODE_UADDC:
    case OPCODE_UBFE:
    case OPCODE_UDIV:
    case OPCODE_UMAD:
    case OPCODE_UMUL:
    case OPCODE_USUBB:
    case OPCODE_DFMA:
    case OPCODE_MSAD:
    case OPCODE_LD_FEEDBACK:
    case OPCODE_LD_RAW_FEEDBACK:
    case OPCODE_LD_UAV_TYPED_FEEDBACK: return 4;
    case OPCODE_BFI:
    case OPCODE_GATHER4_C:
    case OPCODE_GATHER4_PO:
    case OPCODE_IMM_ATOMIC_CMP_EXCH:
    case OPCODE_SAMPLE_C:
    case OPCODE_SAMPLE_C_LZ:
    case OPCODE_SAMPLE_L:
    case OPCODE_SAMPLE_B:
    case OPCODE_SWAPC:
    case OPCODE_GATHER4_FEEDBACK:
    case OPCODE_LD_MS_FEEDBACK:
    case OPCODE_LD_STRUCTURED_FEEDBACK: return 5;
    case OPCODE_GATHER4_PO_C:
    case OPCODE_SAMPLE_D:
    case OPCODE_SAMPLE_CLAMP_FEEDBACK:
    case OPCODE_SAMPLE_C_CLAMP_FEEDBACK:
    case OPCODE_SAMPLE_C_LZ_FEEDBACK:
    case OPCODE_SAMPLE_L_FEEDBACK:
    case OPCODE_SAMPLE_B_CLAMP_FEEDBACK:
    case OPCODE_GATHER4_C_FEEDBACK:
    case OPCODE_GATHER4_PO_FEEDBACK: return 6;
    case OPCODE_SAMPLE_D_CLAMP_FEEDBACK:
    case OPCODE_GATHER4_PO_C_FEEDBACK:
      return 7;

    // custom data doesn't have particular operands
    case OPCODE_CUSTOMDATA:
    default: break;
  }

  RDCERR("Unknown opcode: %u", op);
  return 0xffffffff;
}

bool IsDeclaration(OpcodeType op)
{
  // isDecl means not a real instruction, just a declaration type token
  bool isDecl = false;
  isDecl = isDecl || (op >= OPCODE_DCL_RESOURCE && op <= OPCODE_DCL_GLOBAL_FLAGS);
  isDecl = isDecl || (op >= OPCODE_DCL_STREAM && op <= OPCODE_DCL_RESOURCE_STRUCTURED);
  isDecl = isDecl || (op == OPCODE_DCL_GS_INSTANCE_COUNT);
  isDecl = isDecl || (op == OPCODE_HS_DECLS);
  isDecl = isDecl || (op == OPCODE_CUSTOMDATA);

  return isDecl;
}

bool IsInput(OperandType oper)
{
  switch(oper)
  {
    case TYPE_INPUT:
    case TYPE_INPUT_PRIMITIVEID:
    case TYPE_INPUT_FORK_INSTANCE_ID:
    case TYPE_INPUT_JOIN_INSTANCE_ID:
    case TYPE_INPUT_CONTROL_POINT:
    // this is an input, yes it's confusing.
    case TYPE_OUTPUT_CONTROL_POINT_ID:
    case TYPE_INPUT_PATCH_CONSTANT:
    case TYPE_INPUT_DOMAIN_POINT:
    case TYPE_INPUT_THREAD_ID:
    case TYPE_INPUT_THREAD_GROUP_ID:
    case TYPE_INPUT_THREAD_ID_IN_GROUP:
    case TYPE_INPUT_COVERAGE_MASK:
    case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
    case TYPE_INPUT_GS_INSTANCE_ID: return true;
    default: break;
  }
  return false;
}

bool IsOutput(OperandType oper)
{
  switch(oper)
  {
    case TYPE_OUTPUT:
    case TYPE_OUTPUT_DEPTH:
    case TYPE_OUTPUT_COVERAGE_MASK:
    case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
    case TYPE_OUTPUT_DEPTH_LESS_EQUAL:
    case TYPE_OUTPUT_STENCIL_REF: return true;
    default: break;
  }
  return false;
}

};    // namespace DXBCBytecode
